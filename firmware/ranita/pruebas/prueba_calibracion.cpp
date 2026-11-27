// SIMULACIÓN: prueba la recalibración de la luz en la compu, sin placa ni LDR.
// Se corre con ./correr.sh. Las lecturas son inventadas: no sustituyen probarla en el salón.
#include <cstdio>
#include <cstdlib>

#include "../calibracion_luz.h"

namespace {
int fallas = 0;
int pruebas = 0;

void revisar(bool ok, const char* que) {
  ++pruebas;
  if (!ok) ++fallas;
  std::printf("[SIMULACIÓN] %-66s %s\n", que, ok ? "OK" : "FALLA");
}

// Igual que sensor_sombra.cpp: ¿con esta normal la luz cuenta como destapada?
bool destapada(uint16_t luz, uint16_t normal) {
  const uint16_t tapada = normal * config::LUZ_TAPADA_PCT / 100;
  if (luz >= normal) return true;
  if (luz <= tapada) return false;
  const long nivel = (static_cast<long>(normal) - luz) * 100 / (normal - tapada);
  return nivel < config::ZONA_MUERTA_NIVEL;
}

// Simula `ms` milisegundos con la luz que da `luz(t)`. Devuelve si hubo recalibración de golpe.
template <typename F>
bool correr(CalibracionLuz& c, uint32_t& t, uint32_t ms, F luz) {
  bool recalibro = false;
  for (uint32_t n = 0; n < ms / config::LECTURA_CADA_MS; ++n, t += config::LECTURA_CADA_MS) {
    const uint16_t l = luz(t);
    recalibro |= c.actualizar(l, destapada(l, c.normal()), t);
  }
  return recalibro;
}

uint16_t ruido(uint16_t base, int amplitud) {
  return static_cast<uint16_t>(base + (std::rand() % (2 * amplitud + 1)) - amplitud);
}
}  // namespace

int main() {
  std::srand(7);
  {
    // Se encendió con la mano encima: normal = 600. Al quitarla, la luz real es 2000.
    CalibracionLuz c;
    uint32_t t = 0;
    c.empezar(600, t);
    correr(c, t, 7000, [](uint32_t) { return ruido(2000, 30); });
    revisar(c.normal() > 1900, "encendida tapada: al destaparla la normal sube (en ~5 s)");
  }
  {
    CalibracionLuz c;
    uint32_t t = 0;
    c.empezar(2000, t);
    // Una mano encima 10 s, temblando un poco (es lo que hace un niño).
    const bool golpe = correr(c, t, 10000, [](uint32_t ms) {
      return ruido(static_cast<uint16_t>(700 + (ms / 300 % 2) * 150), 20);
    });
    revisar(!golpe && c.normal() >= 1950, "una mano 10 s no cambia la normal");
    revisar(!destapada(ruido(700, 20), c.normal()), "y la mano sigue contando como sombra");
  }
  {
    CalibracionLuz c;
    uint32_t t = 0;
    c.empezar(2000, t);
    // Apagan la mitad de las luces: la luz baja a 1100 y se queda ahí.
    bool golpe = correr(c, t, 20000, [](uint32_t) { return ruido(1100, 15); });
    revisar(!golpe, "apagan luces: no recalibra antes de 30 s");
    golpe = correr(c, t, 15000, [](uint32_t) { return ruido(1100, 15); });
    revisar(golpe && c.normal() > 1050 && c.normal() < 1150, "apagan luces: a los 30 s esa luz es la normal");
    revisar(destapada(ruido(1100, 15), c.normal()), "y ya no suena sola");
    revisar(!destapada(400, c.normal()), "con la nueva normal una mano sí cuenta");
  }
  {
    CalibracionLuz c;
    uint32_t t = 0;
    c.empezar(2000, t);
    // La luz baja despacio (tarde): 2000 -> 1800 en 60 s.
    correr(c, t, 60000, [](uint32_t ms) { return static_cast<uint16_t>(2000 - ms * 200 / 60000); });
    revisar(c.normal() < 1850, "luz que baja despacio: la normal la sigue");
  }
  {
    CalibracionLuz c;
    uint32_t t = 0;
    c.empezar(2000, t);
    // Alguien parado junto a la mesa: la luz baja 5 %.
    bool alguna = false;
    for (int i = 0; i < 200; ++i) alguna |= !destapada(ruido(1900, 20), c.normal());
    revisar(!alguna, "la sombra de alguien al lado (-5 %) no la hace sonar");
  }
  {
    CalibracionLuz c;
    uint32_t t = 0;
    c.empezar(300, t);
    // Salón oscuro (300) con ruido del ADC: no debe sonar sola.
    correr(c, t, 10000, [](uint32_t) { return ruido(300, 12); });
    bool alguna = false;
    for (int i = 0; i < 200; ++i) alguna |= !destapada(ruido(300, 12), c.normal());
    revisar(!alguna, "con poca luz, el ruido del ADC no la hace sonar");
  }
  {
    // Medido en la placa el 2026-11-26, cada 2 s: normal ~1730, mano, y una lámpara de celular
    // que brinca entre 3078 y 4095; al quitarla, la luz de antes (~1720). Entre lecturas se
    // mueve como se movía la mano: se interpola y se agrega ruido.
    const uint16_t medido[] = {1757, 1743, 1724, 1726, 1704, 1709, 1721, 1707, 1717, 1691, 675, 1735,
                               116, 1609, 670, 1717, 3544, 3775, 1762, 1718, 4095, 4094, 3078,
                               4001, 3422, 1700, 1700, 1719, 1725, 1720, 1718, 1722};
    CalibracionLuz c;
    uint32_t t = 0;
    c.empezar(1720, t);
    const size_t n = sizeof(medido) / sizeof(medido[0]);
    bool sono_mano = false;
    size_t sola_despues = 0;
    for (size_t i = 0; i + 1 < n; ++i) {
      correr(c, t, 2000, [&](uint32_t ms) {
        const uint32_t k = ms % 2000;
        const int l = medido[i] + (static_cast<int>(medido[i + 1]) - medido[i]) * static_cast<int>(k) / 2000;
        return ruido(static_cast<uint16_t>(l < 30 ? 30 : l > 4065 ? 4065 : l), 30);
      });
      if (i == 10) sono_mano = !destapada(675, c.normal());
      if (i >= 26) sola_despues += destapada(medido[i], c.normal()) ? 0 : 1;
    }
    revisar(sono_mano, "medido: la mano si suena");
    revisar(c.normal() < 1900, "medido: la lampara del celular no cambia la luz normal");
    revisar(sola_despues == 0, "medido: al quitar la lampara no suena sola");
  }
  {
    // Lámpara quieta 6 s (sí parece luz del cuarto) y luego vuelve la de antes: regresa enseguida.
    CalibracionLuz c;
    uint32_t t = 0;
    c.empezar(1720, t);
    correr(c, t, 6000, [](uint32_t) { return ruido(3500, 20); });
    revisar(c.normal() > 3300, "una luz fuerte que se queda quieta 5 s si se toma");
    correr(c, t, 3000, [](uint32_t) { return ruido(1720, 20); });
    revisar(c.normal() < 1800, "si vuelve la luz de antes, regresa a ella en ~2 s");
    revisar(!destapada(800, c.normal()), "y una mano sigue contando");
  }
  {
    // Prenden más luces de verdad: se sigue.
    CalibracionLuz c;
    uint32_t t = 0;
    c.empezar(1800, t);
    correr(c, t, 7000, [](uint32_t) { return ruido(3500, 30); });
    revisar(c.normal() > 3400, "mas luz de verdad (3500, quieta): la normal la sigue");
  }
  {
    CalibracionLuz c;
    uint32_t t = 0xFFFFF000u;  // millis() a punto de desbordar
    c.empezar(2000, t);
    const bool golpe = correr(c, t, 35000, [](uint32_t) { return ruido(1000, 10); });
    revisar(golpe, "funciona aunque millis() desborde");
  }
  std::printf("[SIMULACIÓN] %d de %d pruebas de calibración pasaron\n", pruebas - fallas, pruebas);
  return fallas == 0 ? 0 : 1;
}
