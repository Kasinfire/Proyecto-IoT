// Pruebas en la compu de la lógica pura del osito (sin placa, sin radio). Correr con ./correr.sh.
// Todos los mensajes son SIMULACIÓN armados a mano: esto no demuestra que la radio funcione.
#include <cstdio>
#include <cstring>

#include "acciones_dj.h"
#include "cola_eventos.h"
#include "config.h"
#include "contrato_mqtt.h"
#include "director.h"
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
// Sintetizador falso: solo cuenta lo que el director le pide.
class SintetizadorFalso : public Sintetizador {
 public:
  void iniciar() override {}
  void notaOn(uint8_t canal, uint8_t nota, uint8_t velocidad) override {
    ++on[canal];
    ultima_on[canal] = nota;
    ultima_vel[canal] = velocidad;
    if (canal == 10 && nota == 76) {
      ++wood_block;
    }
    if (canal == 10 && velocidad == 110) {
      ++acentos;
    }
    if (canal == 10 && nota == 90) {
      ++pios;
    }
  }
  void notaOff(uint8_t canal, uint8_t nota) override {
    ++off[canal];
    ultima_off[canal] = nota;
  }
  void expresion(uint8_t canal, uint8_t valor) override {
    exp[canal] = valor;
  }
  void volumen(uint8_t) override {}
  void limpiar() {
    for (int i = 0; i < 17; ++i) {
      on[i] = off[i] = 0;
    }
  }
  int on[17] = {};
  uint8_t ultima_vel[17] = {};
  int wood_block = 0;
  int acentos = 0;
  int pios = 0;
  int ultima_off[17] = {};
  int off[17] = {};
  uint8_t ultima_on[17] = {};
  uint8_t exp[17] = {};
};

constexpr uint8_t C_OSITO = 1, C_RANITA = 2, C_GATITO = 3, C_POLLITO = 10;

// Avanza el reloj de 1 en 1 ms. Si se indica, ese animal manda un latido SIMULADO cada 1 s.
void correr(uint32_t& t, uint32_t hasta, int vivo = -1) {
  for (; t < hasta; ++t) {
    if (vivo >= 0 && t % protocolo::LATIDO_CADA_MS == 0) {
      estado_animales::registrarMensaje(static_cast<Nodo>(vivo), Fuente::Simulacion, t);
    }
    director::actualizar(t);
  }
}

interprete::Evento gesto(Nodo nodo, interprete::Clase clase, uint8_t valor, uint8_t vel = 0) {
  const bool es_gesto = clase == interprete::Clase::Nota || valor > 0;
  return {nodo, clase, valor, vel, es_gesto};
}

void pruebasCancion() {
  using interprete::Clase;
  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 7, t);
  correr(t, 4000);
  revisar("cancion: la base del osito suena sola", s.on[C_OSITO] >= 3 && s.on[C_RANITA] == 0);
  revisar("cancion: la base termina a expresion maxima", s.exp[C_OSITO] == 127);

  // SIMULACIÓN: la ranita se conecta y la tapan una vez.
  s.limpiar();
  estado_animales::registrarMensaje(Nodo::Ranita, Fuente::Simulacion, t);
  estado_animales::registrarGesto(Nodo::Ranita, t);
  director::alEvento(gesto(Nodo::Ranita, Clase::Sombra, 60), t);
  const uint32_t gesto_ms = t;
  const int vivo = protocolo::aEntero(Nodo::Ranita);
  correr(t, gesto_ms + 1900, vivo);
  revisar("cancion: con gesto de la ranita entra su parte", s.on[C_RANITA] >= 1);
  revisar("cancion: el gesto no toca notas sueltas", s.ultima_on[C_RANITA] == 64);
  revisar("cancion: 1.9 s tras el gesto sigue a todo volumen", s.exp[C_RANITA] == 127);
  correr(t, gesto_ms + 2000 + 150 + 20, vivo);
  revisar("cancion: sigue conectada (con latido)", estado_animales::estaConectado(Nodo::Ranita, t));
  revisar("cancion: ~2 s tras el ultimo gesto la parte sale (expresion 0)", s.exp[C_RANITA] == 0);
  const int antes = s.on[C_RANITA];
  correr(t, gesto_ms + 4500, vivo);
  revisar("cancion: ya sin gesto no vuelve a sonar", s.on[C_RANITA] == antes);
  revisar("cancion: la base sigue sonando", s.exp[C_OSITO] == 127);

  // SIMULACIÓN: el pollito agitado en Canción toca su ritmo con acentos.
  s.limpiar();
  s.wood_block = 0;
  s.acentos = 0;
  const int pollito = protocolo::aEntero(Nodo::Pollito);
  estado_animales::registrarMensaje(Nodo::Pollito, Fuente::Simulacion, t);
  estado_animales::registrarGesto(Nodo::Pollito, t);
  director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 60), t);
  const uint32_t agito_ms = t;
  correr(t, agito_ms + 1500, pollito);
  revisar("cancion: el ritmo del pollito trae wood block (76)", s.wood_block >= 1);
  revisar("cancion: y golpes acentuados (velocidad 110)", s.acentos >= 1);
  revisar("cancion: la base sigue a 1 por segundo con pulso de 250 ms",
          s.on[C_OSITO] >= 1 && s.on[C_OSITO] <= 2);
  revisar("cancion: con una sola sacudida no hay pio", s.pios == 0);

  // SIMULACIÓN: agitarlo seguido (cada 350 ms, lo más rápido que manda el pollito).
  for (int i = 0; i < 6; ++i) {
    estado_animales::registrarGesto(Nodo::Pollito, t);
    director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 20), t);
    correr(t, t + 350, pollito);
  }
  correr(t, t + 1000, pollito);
  revisar("cancion: agitado seguido, el pollito suma el pio al ritmo", s.pios >= 1);

  revisar("cancion: \"estrellita\" no existe todavia", !director::elegirCancion("estrellita"));
  revisar("cancion: \"prueba\" si existe", director::elegirCancion("prueba"));
}

void pruebasLibre() {
  using interprete::Clase;
  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 7, t);
  director::cambiarModo(protocolo::Modo::Libre, t);
  s.limpiar();
  correr(t, 3000);
  revisar("libre: sin gestos no suena nada (ni la base)", s.on[C_OSITO] == 0);

  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 2, 90), t);
  revisar("libre: gatito mi -> nota 64", s.on[C_GATITO] == 1 && s.ultima_on[C_GATITO] == 64);
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 2, 0), t);
  revisar("libre: al soltar se apaga", s.off[C_GATITO] == 1);
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 3, 90), t);
  revisar("libre: tecla fa toca sol (67), pentatonica", s.ultima_on[C_GATITO] == 67);

  director::alEvento(gesto(Nodo::Ranita, Clase::Sombra, 100), t);
  revisar("libre: ranita tapada -> nota mas aguda (88)", s.ultima_on[C_RANITA] == 88);
  director::alEvento(gesto(Nodo::Ranita, Clase::Sombra, 0), t);
  revisar("libre: ranita destapada -> se apaga", s.off[C_RANITA] == 1);

  // El instrumento depende de cuántas sacudidas seguidas llegan (cada 400 ms aquí).
  director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 20), t);
  revisar("libre: 1 sacudida -> maraca (70) en canal 10",
          s.on[C_POLLITO] == 1 && s.ultima_on[C_POLLITO] == 70);
  const uint8_t vel_una = s.ultima_vel[C_POLLITO];
  correr(t, t + 150);
  revisar("libre: el golpe se apaga solo", s.off[C_POLLITO] == 1);
  correr(t, t + 250);
  director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 20), t);
  revisar("libre: 2 seguidas -> pandereta (54)",
          s.on[C_POLLITO] == 2 && s.ultima_on[C_POLLITO] == 54);
  correr(t, t + 400);
  director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 20), t);
  correr(t, t + 400);
  director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 20), t);
  revisar("libre: 4 seguidas -> pandereta + platillo abierto (46)",
          s.on[C_POLLITO] == 5 && s.ultima_on[C_POLLITO] == 46);
  correr(t, t + 150);
  revisar("libre: se sueltan las dos notas", s.off[C_POLLITO] == 5);
  correr(t, t + 250);
  director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 20), t);
  revisar("libre: 5 seguidas -> ademas un pio (90)",
          s.on[C_POLLITO] == 8 && s.ultima_on[C_POLLITO] == 90);
  revisar("libre: agitar seguido suena mas fuerte", s.ultima_vel[C_POLLITO] > vel_una);
  correr(t, t + 150);
  revisar("libre: se sueltan las tres notas", s.off[C_POLLITO] == 8);
  correr(t, t + 2100);
  director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 20), t);
  revisar("libre: tras 2 s quieto vuelve a la maraca",
          s.on[C_POLLITO] == 9 && s.ultima_on[C_POLLITO] == 70);
  correr(t, t + 150);

  const uint8_t pentatonica[] = {60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84, 86, 88};
  bool todas_en_escala = true;
  for (uint8_t nivel = 1; nivel <= 100; ++nivel) {
    director::alEvento(gesto(Nodo::Ranita, Clase::Sombra, nivel), t);
    bool esta = false;
    for (uint8_t n : pentatonica) {
      esta = esta || s.ultima_on[C_RANITA] == n;
    }
    todas_en_escala = todas_en_escala && esta;
  }
  revisar("libre: los 100 niveles de sombra caen en la pentatonica", todas_en_escala);
}

void pruebasSimon() {
  using interprete::Clase;
  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 12345, t);
  director::cambiarModo(protocolo::Modo::Simon, t);
  protocolo::PayloadLuz luz;
  simon::Resultado resultado;

  correr(t, 5000);
  revisar("simon: sin animales conectados no pide nada", !director::sacarLuz(luz));

  // SIMULACIÓN: solo el gatito está conectado.
  const int vivo = protocolo::aEntero(Nodo::Gatito);
  correr(t, 5100, vivo);
  bool pidio = director::sacarLuz(luz);
  revisar("simon: pide una tecla del gatito",
          pidio && luz.nodo_destino == protocolo::aEntero(Nodo::Gatito) &&
              luz.led < protocolo::NUM_NOTAS);

  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, luz.led, 90), t);
  revisar("simon: tecla correcta -> acierto",
          director::sacarResultado(resultado) && resultado == simon::Resultado::Acierto);

  correr(t, t + 1600, vivo);
  pidio = director::sacarLuz(luz);
  const uint8_t otra = (luz.led + 1) % protocolo::NUM_NOTAS;
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, otra, 90), t);
  revisar("simon: tecla equivocada -> error",
          pidio && director::sacarResultado(resultado) && resultado == simon::Resultado::Error);

  correr(t, t + 1600, vivo);
  pidio = director::sacarLuz(luz);
  correr(t, t + 5100, vivo);
  revisar("simon: sin respuesta en 5 s -> tiempo agotado",
          pidio && director::sacarResultado(resultado) &&
              resultado == simon::Resultado::TiempoAgotado);
  revisar("simon: cuenta 1 acierto y 2 errores", simon::aciertos() == 1 && simon::errores() == 2);
}

void pruebasArranque() {
  SintetizadorFalso s;
  director::iniciar(s, 1, 0);
  int apagados = 0;
  for (int c = 0; c < 17; ++c) {
    apagados += s.off[c];
  }
  revisar("arranque: no apaga notas que nunca sonaron", apagados == 0);
}

void pruebasModos() {
  SintetizadorFalso s;
  director::iniciar(s, 1, 0);
  director::siguienteModo(0);
  const bool libre = director::modo() == protocolo::Modo::Libre;
  director::siguienteModo(0);
  const bool simon = director::modo() == protocolo::Modo::Simon;
  director::siguienteModo(0);
  revisar("boton: cancion -> libre -> simon -> cancion",
          libre && simon && director::modo() == protocolo::Modo::Cancion);
}
void pruebasDj() {
  using acciones_dj::Tipo;
  using interprete::Clase;
  revisar("dj: A -> cancion", acciones_dj::deTecla('A').tipo == Tipo::CambiarModo &&
                                  acciones_dj::deTecla('A').modo == protocolo::Modo::Cancion);
  revisar("dj: B -> libre", acciones_dj::deTecla('B').modo == protocolo::Modo::Libre);
  revisar("dj: C -> simon", acciones_dj::deTecla('C').modo == protocolo::Modo::Simon);
  revisar("dj: 1/2/3 -> ranita/gatito/pollito",
          acciones_dj::deTecla('1').nodo == Nodo::Ranita &&
              acciones_dj::deTecla('2').nodo == Nodo::Gatito &&
              acciones_dj::deTecla('3').tipo == Tipo::AlternarSilencio &&
              acciones_dj::deTecla('3').nodo == Nodo::Pollito);
  revisar("dj: * graba, # play/stop, D borra",
          acciones_dj::deTecla('*').tipo == Tipo::GrabarLoop &&
              acciones_dj::deTecla('#').tipo == Tipo::AlternarLoop &&
              acciones_dj::deTecla('D').tipo == Tipo::BorrarLoop);
  revisar("dj: 7 baja y 9 sube el tempo", acciones_dj::deTecla('7').tempo_pct < 0 &&
                                              acciones_dj::deTecla('9').tempo_pct > 0);
  revisar("dj: 4 eco, 5 redoble, 6 subida, 8 octava, 0 pio",
          acciones_dj::deTecla('4').tipo == Tipo::Eco &&
              acciones_dj::deTecla('5').tipo == Tipo::Redoble &&
              acciones_dj::deTecla('6').tipo == Tipo::Subida &&
              acciones_dj::deTecla('8').tipo == Tipo::Octava &&
              acciones_dj::deTecla('0').tipo == Tipo::Pio);

  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 7, t);
  director::cambiarModo(protocolo::Modo::Libre, t);
  revisar("dj: el osito no se silencia", !director::alternarSilencio(Nodo::Osito));
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 2, 90), t);
  revisar("dj: gatito suena antes de silenciar", s.on[C_GATITO] == 1);
  revisar("dj: tecla 2 silencia al gatito", director::alternarSilencio(Nodo::Gatito));
  revisar("dj: al silenciar se apaga lo que sonaba", s.off[C_GATITO] == 1);
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 4, 90), t);
  revisar("dj: gatito silenciado no toca", s.on[C_GATITO] == 1);
  director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 20), t);
  revisar("dj: los demas siguen sonando", s.on[C_POLLITO] == 1);
  revisar("dj: tecla 2 otra vez lo devuelve", !director::alternarSilencio(Nodo::Gatito));
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 4, 90), t);
  revisar("dj: gatito vuelve a tocar", s.on[C_GATITO] == 2);

  // SIMULACIÓN: en Canción, la ranita activa pero silenciada sale con fade.
  director::cambiarModo(protocolo::Modo::Cancion, t);
  const int vivo = protocolo::aEntero(Nodo::Ranita);
  estado_animales::registrarMensaje(Nodo::Ranita, Fuente::Simulacion, t);
  estado_animales::registrarGesto(Nodo::Ranita, t);
  correr(t, t + 500, vivo);
  revisar("dj: cancion, ranita activa suena", s.exp[C_RANITA] == 127);
  director::alternarSilencio(Nodo::Ranita);
  estado_animales::registrarGesto(Nodo::Ranita, t);
  correr(t, t + 400, vivo);
  revisar("dj: cancion, ranita silenciada baja a 0 aunque siga activa", s.exp[C_RANITA] == 0);
  revisar("dj: cancion, la base sigue", s.exp[C_OSITO] == 127);
}

void pruebasLooper() {
  using interprete::Clase;
  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 7, t);
  director::cambiarModo(protocolo::Modo::Libre, t);
  s.limpiar();
  const uint32_t pulso = 250;  // "prueba" con tempo normal

  director::grabarLoop();
  correr(t, t + 10);
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 2, 90), t);  // mi, antes del 1er pulso
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 2, 0), t);
  correr(t, t + pulso * 4 + 20);
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 4, 90), t);  // sol, a tiempo
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 4, 0), t);
  revisar("looper: mientras graba suena el metronomo", s.on[C_POLLITO] >= 2);
  size_t notas = 0;
  revisar("looper: aun no termina de grabar", !director::sacarFinGrabacion(notas));
  correr(t, t + pulso * 13);
  revisar("looper: a los 16 pulsos termina", director::sacarFinGrabacion(notas) && notas == 2);
  s.limpiar();
  correr(t, t + pulso * 16);
  revisar("looper: una vuelta repite las 2 notas solo", s.on[C_GATITO] == 2);
  revisar("looper: y ya sin metronomo", s.on[C_POLLITO] == 0);
  revisar("looper: cada nota del loop se suelta", s.off[C_GATITO] == 2);

  // Encimar: el pollito agitado en otra vuelta se suma al loop.
  director::grabarLoop();
  director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 20), t);
  correr(t, t + pulso * 16 + 10);
  s.limpiar();
  correr(t, t + pulso * 16);
  revisar("looper: encimar suma la capa del pollito",
          s.on[C_GATITO] == 2 && s.on[C_POLLITO] == 1);

  director::alternarSilencio(Nodo::Gatito);
  s.limpiar();
  correr(t, t + pulso * 16);
  revisar("looper: silenciar al gatito calla su parte del loop",
          s.on[C_GATITO] == 0 && s.on[C_POLLITO] == 1);
  director::alternarSilencio(Nodo::Gatito);

  revisar("looper: # lo para", !director::alternarLoop());
  s.limpiar();
  correr(t, t + pulso * 16);
  revisar("looper: parado no suena", s.on[C_GATITO] == 0);
  revisar("looper: # lo vuelve a poner", director::alternarLoop());
  director::cambiarModo(protocolo::Modo::Cancion, t);
  s.limpiar();
  correr(t, t + pulso * 16);
  revisar("looper: en Cancion no suena el loop", s.on[C_GATITO] == 0);
  director::cambiarModo(protocolo::Modo::Libre, t);
  correr(t, t + pulso * 16);
  revisar("looper: de vuelta en Libre sigue ahi", s.on[C_GATITO] == 2);
  director::borrarLoop();
  s.limpiar();
  correr(t, t + pulso * 16);
  revisar("looper: D lo borra", s.on[C_GATITO] == 0 && !director::alternarLoop());
}

void pruebasLooperCancelar() {
  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 7, t);
  director::cambiarModo(protocolo::Modo::Libre, t);
  director::grabarLoop();
  correr(t, t + 600);
  director::alternarLoop();  // # con la grabación nueva todavía vacía
  s.limpiar();
  correr(t, t + 1000);
  revisar("looper: # cancela una grabacion vacia (sin metronomo)", s.on[C_POLLITO] == 0);
}

void pruebasEfectos() {
  using interprete::Clase;
  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 7, t);
  director::cambiarModo(protocolo::Modo::Libre, t);
  s.limpiar();
  const uint32_t pulso = 250;

  revisar("efectos: octava arriba", director::alternarOctava());
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 2, 90), t);  // mi = 64
  revisar("efectos: con octava el mi suena 76", s.ultima_on[C_GATITO] == 76);
  director::alternarOctava();
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 2, 0), t);
  revisar("efectos: se suelta el 76 aunque ya se quito la octava",
          s.ultima_off[C_GATITO] == 76);
  director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 20), t);
  revisar("efectos: la percusion no cambia de octava", s.ultima_on[C_POLLITO] == 70);

  s.limpiar();
  revisar("efectos: eco encendido", director::alternarEco());
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 0, 100), t);
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 0, 0), t);
  correr(t, t + pulso * 8);
  revisar("efectos: eco repite la nota 3 veces", s.on[C_GATITO] == 4);
  revisar("efectos: cada eco mas bajito", s.ultima_vel[C_GATITO] < 100 * 60 / 100);
  revisar("efectos: eco apagado", !director::alternarEco());

  s.limpiar();
  director::redoble(t);
  correr(t, t + pulso * 4 + 20);
  revisar("efectos: redoble toca varios golpes de tambor", s.on[16] >= 8);
  revisar("efectos: y remata con platillo", s.ultima_on[16] == 49);

  s.limpiar();
  director::subida(t);
  correr(t, t + pulso * 4);
  const uint8_t a_la_mitad = s.ultima_on[15];
  correr(t, t + pulso * 4 + 20);
  revisar("efectos: la subida va subiendo", a_la_mitad > 60 && a_la_mitad < 96);
  revisar("efectos: termina con la caida (do grave)", s.ultima_on[15] == 36);
  revisar("efectos: y platillo", s.ultima_on[16] == 49);

  s.limpiar();
  director::pio();
  revisar("efectos: 0 toca el pio en el canal del DJ", s.ultima_on[16] == 90);

  // En Canción los efectos también suenan: van en canales que el fade no toca.
  director::cambiarModo(protocolo::Modo::Cancion, t);
  s.limpiar();
  director::redoble(t);
  correr(t, t + pulso * 4 + 20);
  revisar("efectos: el redoble suena tambien en Cancion", s.on[16] >= 8);
}

void pruebasTempo() {
  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 7, t);
  correr(t, t + 4000);
  const int normal = s.on[C_OSITO];
  revisar("tempo: empieza en 100%", director::tempo() == 100);
  for (int i = 0; i < 20; ++i) {
    director::cambiarTempo(10);
  }
  revisar("tempo: no pasa de 200%", director::tempo() == 200);
  s.limpiar();
  correr(t, t + 4000);
  revisar("tempo: al 200% la base suena el doble", s.on[C_OSITO] >= normal * 2 - 1);
  for (int i = 0; i < 30; ++i) {
    director::cambiarTempo(-10);
  }
  revisar("tempo: no baja de 50%", director::tempo() == 50);
}

void pruebasContratoMqtt() {
  using interprete::Clase;
  char texto[128];
  contrato_mqtt::topicoEvento(Nodo::Gatito, texto, sizeof(texto));
  revisar("mqtt: topico orquesta/gatito/evento", strcmp(texto, "orquesta/gatito/evento") == 0);
  contrato_mqtt::topicoEstado(Nodo::Pollito, texto, sizeof(texto));
  revisar("mqtt: topico orquesta/pollito/estado", strcmp(texto, "orquesta/pollito/estado") == 0);

  contrato_mqtt::armarEvento(1532, 1762900000, gesto(Nodo::Gatito, Clase::Nota, 2, 90), texto,
                             sizeof(texto));
  revisar("mqtt: evento igual al ejemplo de la especificacion",
          strcmp(texto,
                 "{\"seq\":1532,\"t\":1762900000,\"nodo\":\"gatito\",\"tipo\":\"nota\","
                 "\"nota\":\"mi\",\"vel\":90}") == 0);
  char corto[20];
  revisar("mqtt: si no cabe el JSON, no se arma a medias",
          !contrato_mqtt::armarEvento(1, 0, gesto(Nodo::Gatito, Clase::Nota, 2, 90), corto,
                                      sizeof(corto)));

  contrato_mqtt::PedidoModo pedido;
  bool leyo = contrato_mqtt::leerPedidoModo(
      "{\"id\": 7, \"modo\": \"simon\", \"cancion\": \"estrellita\"}", pedido);
  revisar("mqtt: lee pedido de modo con cancion",
          leyo && pedido.id == 7 && pedido.modo_valido && pedido.modo == protocolo::Modo::Simon &&
              pedido.trae_cancion && strcmp(pedido.cancion, "estrellita") == 0);
  leyo = contrato_mqtt::leerPedidoModo("{\"id\":8,\"modo\":\"karaoke\"}", pedido);
  revisar("mqtt: modo desconocido -> modo_valido=false", leyo && !pedido.modo_valido);
  revisar("mqtt: pedido sin id -> se ignora",
          !contrato_mqtt::leerPedidoModo("{\"modo\":\"libre\"}", pedido));

  contrato_mqtt::armarAckModo(7, true, protocolo::Modo::Cancion, texto, sizeof(texto));
  revisar("mqtt: ack {\"id\":7,\"ok\":true,\"modo\":\"cancion\"}",
          strcmp(texto, "{\"id\":7,\"ok\":true,\"modo\":\"cancion\"}") == 0);
  uint32_t hasta = 0;
  revisar("mqtt: lee sync ack hasta_seq",
          contrato_mqtt::leerSyncAck("{\"hasta_seq\":42}", hasta) && hasta == 42);
  revisar("mqtt: sync ack negativo -> invalido",
          !contrato_mqtt::leerSyncAck("{\"hasta_seq\":-1}", hasta));
  // Mensajes nuevos para la interfaz web (nodered/README.md del PR #13).
  char buf[config::EVENTO_JSON_MAX];
  revisar("mqtt: evento de modo del osito con origen",
          contrato_mqtt::armarEventoModo(9, 0, protocolo::Modo::Libre, "prueba",
                                         contrato_mqtt::Origen::Teclado, buf, sizeof(buf)) &&
              strcmp(buf, "{\"seq\":9,\"t\":0,\"nodo\":\"osito\",\"tipo\":\"modo\","
                          "\"modo\":\"libre\",\"cancion\":\"prueba\",\"origen\":\"teclado\"}") == 0);
  revisar("mqtt: simon pedido",
          contrato_mqtt::armarEventoSimon(10, 0, Nodo::Ranita, simon::Resultado::Nada, buf,
                                          sizeof(buf)) &&
              strstr(buf, "\"pedido\":\"ranita\"}") != nullptr);
  revisar("mqtt: simon tiempo agotado se llama \"tiempo\"",
          contrato_mqtt::armarEventoSimon(11, 0, Nodo::Pollito, simon::Resultado::TiempoAgotado,
                                          buf, sizeof(buf)) &&
              strstr(buf, "\"pedido\":\"pollito\",\"resultado\":\"tiempo\"}") != nullptr);
  revisar("mqtt: volumen", contrato_mqtt::armarVolumen(70, buf, sizeof(buf)) &&
                               strcmp(buf, "{\"valor\":70}") == 0);
  revisar("mqtt: estado de la cola", contrato_mqtt::armarCola(37, 2, buf, sizeof(buf)) &&
                                         strcmp(buf, "{\"pendientes\":37,\"descartados\":2}") == 0);
  bool silenciar = false;
  revisar("mqtt: aviso silenciar true",
          contrato_mqtt::leerAviso("{\"silenciar\": true}", silenciar) && silenciar);
  revisar("mqtt: aviso silenciar false",
          contrato_mqtt::leerAviso("{\"silenciar\":false}", silenciar) && !silenciar);
  revisar("mqtt: aviso sin el campo se ignora", !contrato_mqtt::leerAviso("{}", silenciar));
}

void pruebasColaEventos() {
  cola_eventos::reiniciar();
  for (int i = 0; i < 5; ++i) {
    cola_eventos::agregar(cola_eventos::reservarSeq(), 2, "{}");
  }
  revisar("cola: 5 eventos pendientes", cola_eventos::pendientes() == 5);
  revisar("cola: un acuse antes de publicar no borra nada",
          cola_eventos::confirmarHasta(5) == 0 && cola_eventos::pendientes() == 5);

  uint32_t seq = 0;
  uint8_t nodo = 0;
  const char* json = nullptr;
  for (int i = 0; i < 3 && cola_eventos::siguientePorPublicar(seq, nodo, json); ++i) {
    cola_eventos::marcarPublicado(seq);  // se publican 1, 2 y 3
  }
  revisar("cola: publicar no borra (falta el acuse)", cola_eventos::pendientes() == 5);
  revisar("cola: acuse hasta 2 borra 1 y 2", cola_eventos::confirmarHasta(2) == 2 &&
                                                 cola_eventos::pendientes() == 3);
  revisar("cola: acuse hasta 5 solo borra lo publicado (el 3)",
          cola_eventos::confirmarHasta(5) == 1 && cola_eventos::pendientes() == 2);
  revisar("cola: sigue el 4, en orden",
          cola_eventos::siguientePorPublicar(seq, nodo, json) && seq == 4 && nodo == 2);

  cola_eventos::marcarPublicado(4);
  cola_eventos::republicarTodo();
  revisar("cola: al reconectar se republica desde el mas viejo",
          cola_eventos::siguientePorPublicar(seq, nodo, json) && seq == 4);

  cola_eventos::reiniciar();
  for (size_t i = 0; i < config::CAPACIDAD_COLA_MQTT + 3; ++i) {
    cola_eventos::agregar(cola_eventos::reservarSeq(), 1, "{}");
  }
  revisar("cola: llena -> descarta los 3 mas viejos",
          cola_eventos::descartados() == 3 &&
              cola_eventos::pendientes() == config::CAPACIDAD_COLA_MQTT &&
              cola_eventos::siguientePorPublicar(seq, nodo, json) && seq == 4);
}
}  // namespace

int main() {
  printf("[SIMULACIÓN] Pruebas de logica del osito en la compu (sin placa)\n");
  pruebasArranque();  // primero: necesita el director recién encendido
  pruebasInterprete();
  pruebasConexion();
  pruebasActividad();
  pruebasCambios();
  pruebasAlerta();
  pruebasAntirrebote();
  pruebasCancion();
  pruebasLibre();
  pruebasSimon();
  pruebasModos();
  pruebasDj();
  pruebasLooper();
  pruebasLooperCancelar();
  pruebasTempo();
  pruebasEfectos();
  pruebasContratoMqtt();
  pruebasColaEventos();
  printf("[SIMULACIÓN] %d de %d pruebas pasaron\n", pruebas - fallos, pruebas);
  return fallos == 0 ? 0 : 1;
}
