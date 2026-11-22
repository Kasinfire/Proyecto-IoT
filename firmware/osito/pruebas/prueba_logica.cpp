// Pruebas en la compu de la lógica pura del osito (sin placa, sin radio). Correr con ./correr.sh.
// Todos los mensajes son SIMULACIÓN armados a mano: esto no demuestra que la radio funcione.
#include <cstdio>
#include <cstring>

#include "estado_animales.h"
#include "filtro_boton.h"
#include "interprete.h"

namespace {
using estado_animales::Fuente;
using protocolo::Nodo;

int fallos = 0;
int pruebas = 0;

void revisar(const char* nombre, bool paso) {
  ++pruebas;
  if (!paso) {
    ++fallos;
  }
  printf("[SIMULACIÓN] %-58s %s\n", nombre, paso ? "OK" : "FALLO");
}

template <typename T>
enlace::Mensaje mensaje(Nodo nodo, const T& payload) {
  enlace::Mensaje m = {};
  m.cabecera = {protocolo::VERSION, protocolo::aEntero(protocolo::TipoDe<T>::valor),
                protocolo::aEntero(nodo), 0, 0};
  memcpy(m.payload, &payload, sizeof(T));
  m.tamano = sizeof(T);
  return m;
}

enlace::Mensaje latido(Nodo nodo) {
  enlace::Mensaje m = {};
  m.cabecera = {protocolo::VERSION, protocolo::aEntero(protocolo::TipoMensaje::Latido),
                protocolo::aEntero(nodo), 0, 0};
  return m;
}

void pruebasInterprete() {
  using interprete::Clase;
  using interprete::interpretar;

  auto e = interpretar(mensaje(Nodo::Ranita, protocolo::PayloadSombra{60}));
  revisar("ranita sombra 60 -> Sombra, cuenta como gesto",
          e.clase == Clase::Sombra && e.valor == 60 && e.es_gesto);
  e = interpretar(mensaje(Nodo::Ranita, protocolo::PayloadSombra{0}));
  revisar("ranita sombra 0 -> no es gesto (la mano se fue)",
          e.clase == Clase::Sombra && !e.es_gesto);
  e = interpretar(mensaje(Nodo::Ranita, protocolo::PayloadSombra{101}));
  revisar("ranita sombra 101 -> invalido (fuera de 0..100)", e.clase == Clase::Invalido);

  e = interpretar(mensaje(Nodo::Gatito, protocolo::PayloadNota{2, 90}));
  revisar("gatito nota mi vel 90 -> Nota", e.clase == Clase::Nota && e.valor == 2 &&
                                              e.velocidad == 90 && e.es_gesto);
  e = interpretar(mensaje(Nodo::Gatito, protocolo::PayloadNota{2, protocolo::VELOCIDAD_SOLTO}));
  revisar("gatito nota mi vel 0 -> Nota soltada", e.clase == Clase::Nota && e.velocidad == 0);
  e = interpretar(mensaje(Nodo::Gatito, protocolo::PayloadNota{7, 90}));
  revisar("gatito nota 7 -> invalido (solo do..si)", e.clase == Clase::Invalido);
  e = interpretar(mensaje(Nodo::Gatito, protocolo::PayloadNota{0, 128}));
  revisar("gatito velocidad 128 -> invalido", e.clase == Clase::Invalido);

  e = interpretar(mensaje(Nodo::Pollito, protocolo::PayloadAgitar{70}));
  revisar("pollito agitar 70 -> Agitar, gesto",
          e.clase == Clase::Agitar && e.valor == 70 && e.es_gesto);

  revisar("ranita mandando Nota -> invalido (no es su gesto)",
          interpretar(mensaje(Nodo::Ranita, protocolo::PayloadNota{0, 90})).clase ==
              Clase::Invalido);
  revisar("pollito mandando Sombra -> invalido",
          interpretar(mensaje(Nodo::Pollito, protocolo::PayloadSombra{50})).clase ==
              Clase::Invalido);
  revisar("latido sin payload -> Latido",
          interpretar(latido(Nodo::Gatito)).clase == Clase::Latido);

  char texto[48];
  interprete::describir(interpretar(mensaje(Nodo::Gatito, protocolo::PayloadNota{4, 80})), texto,
                        sizeof(texto));
  revisar("describir -> \"gatito nota sol vel=80\"", strcmp(texto, "gatito nota sol vel=80") == 0);
}

void pruebasConexion() {
  using namespace estado_animales;
  reiniciar();
  revisar("al arrancar nadie esta conectado", !estaConectado(Nodo::Ranita, 0));

  // Latidos de la ranita cada 1 s, por radio.
  for (uint32_t t = 1000; t <= 4000; t += protocolo::LATIDO_CADA_MS) {
    registrarMensaje(Nodo::Ranita, Fuente::Radio, t);
  }
  revisar("ranita con latidos -> conectada por RADIO",
          estaConectado(Nodo::Ranita, Fuente::Radio, 4500));
  revisar("ranita no aparece como SIMULACIÓN",
          !estaConectado(Nodo::Ranita, Fuente::Simulacion, 4500));
  revisar("4.9 s sin latido -> sigue conectada", estaConectado(Nodo::Ranita, 4000 + 4999));
  revisar("5 s sin latido -> desconectada", !estaConectado(Nodo::Ranita, 4000 + 5000));

  registrarMensaje(Nodo::Gatito, Fuente::Simulacion, 10000);
  revisar("gatito simulado -> conectado solo como SIMULACIÓN",
          estaConectado(Nodo::Gatito, Fuente::Simulacion, 10100) &&
              !estaConectado(Nodo::Gatito, Fuente::Radio, 10100));
  revisar("el osito nunca cuenta como animal",
          (registrarMensaje(Nodo::Osito, Fuente::Radio, 10000),
           !estaConectado(Nodo::Osito, 10100)));
}

void pruebasActividad() {
  using namespace estado_animales;
  reiniciar();
  registrarMensaje(Nodo::Pollito, Fuente::Simulacion, 1000);
  registrarGesto(Nodo::Pollito, 1000);
  revisar("pollito agitado -> activo", estaActivo(Nodo::Pollito, 1000));
  revisar("1.9 s despues -> sigue activo", estaActivo(Nodo::Pollito, 2999));
  revisar("2 s despues -> ya no", !estaActivo(Nodo::Pollito, 3000));

  registrarMensaje(Nodo::Gatito, Fuente::Simulacion, 1000);
  registrarGesto(Nodo::Gatito, 1000);
  sostenerTecla(Nodo::Gatito, 3, true);
  registrarMensaje(Nodo::Gatito, Fuente::Simulacion, 4000);  // latido
  revisar("tecla sostenida 3.5 s -> sigue activo", estaActivo(Nodo::Gatito, 4500));
  sostenerTecla(Nodo::Gatito, 3, false);
  registrarGesto(Nodo::Gatito, 4500);
  revisar("solto la tecla -> 2 s de gracia", estaActivo(Nodo::Gatito, 6400));
  revisar("pasados los 2 s -> inactivo", !estaActivo(Nodo::Gatito, 6600));

  registrarGesto(Nodo::Ranita, 7000);
  revisar("gesto sin conexion -> no activo", !estaActivo(Nodo::Ranita, 7000));
}

void pruebasCambios() {
  using namespace estado_animales;
  reiniciar();
  CambioConexion cambio;
  registrarMensaje(Nodo::Ranita, Fuente::Radio, 1000);
  bool hubo = sacarCambio(1000, cambio);
  revisar("cambio: ranita se conecto por RADIO",
          hubo && cambio.nodo == Nodo::Ranita && cambio.fuente == Fuente::Radio &&
              cambio.conectado);
  revisar("sin nada nuevo -> sin cambio", !sacarCambio(2000, cambio));
  hubo = sacarCambio(6000, cambio);
  revisar("cambio: ranita se desconecto tras 5 s", hubo && !cambio.conectado);
}
void pruebasAlerta() {
  using namespace estado_animales;
  reiniciar();
  revisar("alerta: animal que nunca aparecio no la dispara", !algunoSeDesconecto(10000));
  registrarMensaje(Nodo::Ranita, Fuente::Radio, 1000);
  revisar("alerta: ranita conectada -> sin alerta", !algunoSeDesconecto(2000));
  revisar("alerta: ranita sin latido 5 s -> alerta", algunoSeDesconecto(6000));
  registrarMensaje(Nodo::Ranita, Fuente::Radio, 6500);
  revisar("alerta: vuelve el latido -> se apaga", !algunoSeDesconecto(6600));
}

void pruebasAntirrebote() {
  FiltroBoton boton(30);
  boton.iniciar(false, 0);
  // Un contacto real: rebota 5 ms en cada borde.
  uint32_t t = 100;
  int pulsaciones = 0;
  for (int i = 0; i < 20; ++i) {
    for (int r = 0; r < 5; ++r, ++t) {
      boton.actualizar(r % 2 == 0, t);
      pulsaciones += boton.sePresiono();
    }
    for (int k = 0; k < 60; ++k, ++t) {
      boton.actualizar(true, t);
      pulsaciones += boton.sePresiono();
    }
    for (int r = 0; r < 5; ++r, ++t) {
      boton.actualizar(r % 2 == 1, t);
      pulsaciones += boton.sePresiono();
    }
    for (int k = 0; k < 60; ++k, ++t) {
      boton.actualizar(false, t);
      pulsaciones += boton.sePresiono();
    }
  }
  char nombre[64];
  snprintf(nombre, sizeof(nombre), "antirrebote: 20 pulsaciones con rebote -> %d", pulsaciones);
  revisar(nombre, pulsaciones == 20);

  FiltroBoton ruido(30);
  ruido.iniciar(false, 0);
  int falsas = 0;
  for (uint32_t k = 0; k < 500; ++k) {
    ruido.actualizar(k % 20 < 10, k);  // pulsos de 10 ms, mas cortos que 30 ms
    falsas += ruido.sePresiono();
  }
  revisar("antirrebote: picos de 10 ms no cuentan", falsas == 0);

  FiltroBoton arranque(30);
  arranque.iniciar(true, 0);
  arranque.actualizar(true, 100);
  revisar("antirrebote: presionado al arrancar no cuenta", !arranque.sePresiono());
}
}  // namespace

int main() {
  printf("[SIMULACIÓN] Pruebas de logica del osito en la compu (sin placa)\n");
  pruebasInterprete();
  pruebasConexion();
  pruebasActividad();
  pruebasCambios();
  pruebasAlerta();
  pruebasAntirrebote();
  printf("[SIMULACIÓN] %d de %d pruebas pasaron\n", pruebas - fallos, pruebas);
  return fallos == 0 ? 0 : 1;
}
