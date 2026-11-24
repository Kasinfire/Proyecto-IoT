#include "acompanamiento.h"

#include <cstddef>

#include "config.h"

namespace acompanamiento {
namespace {
constexpr uint8_t PASOS = 16;
constexpr uint8_t PASOS_COMPAS = 8;
constexpr uint8_t NUM_RITMOS = 4;
constexpr size_t NOTAS_ACORDE = 3;
constexpr uint8_t NUM_ACORDES = 4;

struct Instrumento {
  uint8_t nota;
  uint8_t velocidad;
  const char* pasos;  // 'x' = golpe
};
constexpr size_t INSTRUMENTOS = 3;
// Dos compases de 8 pulsos cada uno.
constexpr Instrumento RITMOS[NUM_RITMOS][INSTRUMENTOS] = {
    {{0, 0, "................"}, {0, 0, "................"}, {0, 0, "................"}},
    {{config::NOTA_BOMBO, 80, "x.......x......."},
     {config::NOTA_TAMBOR, 45, "............x..."},
     {config::NOTA_PLATILLO_CERRADO, 40, "....x...x...x..."}},
    {{config::NOTA_BOMBO, 95, "x.....x.x......."},
     {config::NOTA_TAMBOR, 80, "....x.......x..."},
     {config::NOTA_PLATILLO_CERRADO, 45, "x.x.x.x.x.x.x.x."}},
    {{config::NOTA_BOMBO, 90, "x...x...x...x..."},
     {config::NOTA_CENCERRO, 45, "..x...x...x...x."},
     {config::NOTA_PLATILLO_CERRADO, 50, "x.xxx.xxx.xxx.xx"}},
};
// Do, la menor, fa y sol: van bien con la pentatónica de do que tocan los animales.
constexpr uint8_t ACORDES[NUM_ACORDES][NOTAS_ACORDE] = {
    {60, 64, 67}, {57, 60, 64}, {53, 57, 60}, {55, 59, 62}};

uint8_t paso = 0;
uint8_t compas = 0;
Ritmo ritmo = Ritmo::Apagado;
Ritmo ritmo_pedido = Ritmo::Apagado;
bool acordes = false;
bool acordes_pedidos = false;
const uint8_t* sonando = nullptr;

void soltarAcorde(Sintetizador& salida) {
  if (sonando == nullptr) {
    return;
  }
  for (size_t i = 0; i < NOTAS_ACORDE; ++i) {
    salida.notaOff(config::CANAL_DJ_ACORDES, sonando[i]);
  }
  sonando = nullptr;
}
}  // namespace

void reiniciar() {
  paso = 0;
  compas = 0;
  ritmo = ritmo_pedido = Ritmo::Apagado;
  acordes = acordes_pedidos = false;
  sonando = nullptr;
}

Ritmo siguienteRitmo() {
  ritmo_pedido = static_cast<Ritmo>((static_cast<uint8_t>(ritmo_pedido) + 1) % NUM_RITMOS);
  return ritmo_pedido;
}

bool alternarAcordes() {
  acordes_pedidos = !acordes_pedidos;
  return acordes_pedidos;
}

Ritmo ritmoPedido() {
  return ritmo_pedido;
}

bool acordesPedidos() {
  return acordes_pedidos;
}

bool encendido() {
  return ritmo != Ritmo::Apagado || acordes;
}

void alPulso(Sintetizador& salida) {
  if (paso % PASOS_COMPAS == 0) {
    ritmo = ritmo_pedido;
    soltarAcorde(salida);
    if (acordes_pedidos && !acordes) {
      compas = 0;  // siempre entran por do
    }
    acordes = acordes_pedidos;
    if (acordes) {
      sonando = ACORDES[compas];
      for (size_t i = 0; i < NOTAS_ACORDE; ++i) {
        salida.notaOn(config::CANAL_DJ_ACORDES, sonando[i], config::VELOCIDAD_ACORDES);
      }
      compas = (compas + 1) % NUM_ACORDES;
    }
  }
  for (const Instrumento& instrumento : RITMOS[static_cast<uint8_t>(ritmo)]) {
    if (instrumento.pasos[paso] == 'x') {
      salida.notaOn(config::CANAL_DJ_PERCUSION, instrumento.nota, instrumento.velocidad);
      salida.notaOff(config::CANAL_DJ_PERCUSION, instrumento.nota);  // el golpe suena completo
    }
  }
  paso = (paso + 1) % PASOS;
}

void callar(Sintetizador& salida) {
  soltarAcorde(salida);
  paso = 0;
  compas = 0;
}
}  // namespace acompanamiento
