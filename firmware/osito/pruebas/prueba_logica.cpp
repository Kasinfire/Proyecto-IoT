// Pruebas en la compu de la lógica pura del osito (sin placa, sin radio). Correr con ./correr.sh.
// Todos los mensajes son SIMULACIÓN armados a mano: esto no demuestra que la radio funcione.
#include <cstdio>
#include <cstring>

#include "acciones_dj.h"
#include "cancion_recibida.h"
#include "panel_json.h"
#include "cola_eventos.h"
#include "config.h"
#include "contrato_mqtt.h"
#include "director.h"
#include "estado_animales.h"
#include "filtro_boton.h"
#include "pulsacion_larga.h"
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
  director::iniciar(s, 7, t, "prueba");
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

  revisar("cancion: una que no existe se rechaza", !director::elegirCancion("no-existe"));
  revisar("cancion: \"estrellita\" si existe", director::elegirCancion("estrellita"));
  revisar("cancion: \"prueba\" si existe", director::elegirCancion("prueba"));
}

void pruebasLibre() {
  using interprete::Clase;
  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 7, t, "prueba");
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

// Deja correr el juego hasta que el niño deba responder (o se acabe el plazo).
void esperarTurno(uint32_t& t, int vivo, int& luces) {
  luces = 0;
  const uint32_t limite = t + 15000;
  bool reto = false;
  uint32_t ultima = t;
  protocolo::PayloadLuz luz;
  while (t < limite) {
    correr(t, t + 1, vivo);
    if (director::sacarLuz(luz)) {
      reto = true;
      ++luces;
      ultima = t;
    }
    if (reto && t - ultima > static_cast<uint32_t>(luz.duracion_ms) +
                              config::SIMON_HUECO_MS + 50) {
      return;
    }
  }
}

// El gatito toca la secuencia completa (o se equivoca en el último paso).
void responderGatito(uint32_t& t, int vivo, bool bien) {
  const size_t largo = simon::largo();
  for (size_t i = 0; i < largo; ++i) {
    uint8_t tecla = simon::pasoDe(i).led;
    if (!bien && i + 1 == largo) {
      tecla = (tecla + 1) % protocolo::NUM_NOTAS;
    }
    director::alEvento(gesto(Nodo::Gatito, interprete::Clase::Nota, tecla, 90), t);
    correr(t, t + 100, vivo);
    director::alEvento(gesto(Nodo::Gatito, interprete::Clase::Nota, tecla, 0), t);
    correr(t, t + 100, vivo);
  }
}

void pruebasSimon() {
  using interprete::Clase;
  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 12345, t, "prueba");
  director::cambiarModo(protocolo::Modo::Simon, t);
  protocolo::PayloadLuz luz;
  simon::Resultado resultado;
  uint8_t nivel = 0;
  Nodo pedido = Nodo::Osito;
  int luces = 0;

  correr(t, 5000);
  revisar("simon: sin animales conectados no pide nada", !director::sacarLuz(luz));

  // SIMULACIÓN: solo el gatito está conectado.
  const int vivo = protocolo::aEntero(Nodo::Gatito);
  s.limpiar();
  correr(t, 5100, vivo);
  revisar("simon: pide una tecla del gatito",
          director::sacarLuz(luz) && luz.nodo_destino == protocolo::aEntero(Nodo::Gatito) &&
              luz.led < protocolo::NUM_NOTAS);
  revisar("simon: la nota suena junto con la luz", s.on[C_GATITO] == 1);
  const uint8_t nota_pista = s.ultima_on[C_GATITO];
  revisar("simon: avisa que empezo un reto de nivel 1",
          director::sacarReto(pedido, nivel) && pedido == Nodo::Gatito && nivel == 1);
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, (luz.led + 1) % 7, 90), t);
  revisar("simon: tocar mientras se muestra no cuenta", !director::sacarResultado(resultado, nivel));
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, (luz.led + 1) % 7, 0), t);
  correr(t, t + config::SIMON_LUZ_MS + config::SIMON_HUECO_MS + 10, vivo);
  revisar("simon: la nota de la pista se suelta", s.off[C_GATITO] >= 2);

  s.limpiar();
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, luz.led, 90), t);
  revisar("simon: tecla correcta -> acierto", director::sacarResultado(resultado, nivel) &&
                                                  resultado == simon::Resultado::Acierto &&
                                                  nivel == 1);
  revisar("simon: suena la misma nota que la pista", s.ultima_on[C_GATITO] == nota_pista);
  correr(t, t + 300, vivo);
  revisar("simon: el tono espera a que se oiga lo que se toco", s.on[C_OSITO] == 0);
  correr(t, t + 600, vivo);
  revisar("simon: al acertar suena un arpegio", s.on[C_OSITO] == 4 && s.ultima_on[C_OSITO] == 84);
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, luz.led, 0), t);

  const simon::Paso primero = simon::pasoDe(0);
  esperarTurno(t, vivo, luces);
  revisar("simon: nivel 2 muestra 2 pasos", simon::nivel() == 2 && luces == 2);
  revisar("simon: conserva el primero y agrega otro distinto",
          simon::pasoDe(0).led == primero.led && simon::pasoDe(1).led != primero.led);
  director::sacarReto(pedido, nivel);

  const uint8_t segundo = simon::pasoDe(1).led;
  responderGatito(t, vivo, false);
  revisar("simon: fallar el segundo paso -> error",
          director::sacarResultado(resultado, nivel) && resultado == simon::Resultado::Error);
  esperarTurno(t, vivo, luces);
  revisar("simon: al primer fallo repite el mismo reto",
          simon::nivel() == 2 && simon::pasoDe(1).led == segundo && luces == 2);
  correr(t, t + config::SIMON_ESPERA_MS + 100, vivo);
  revisar("simon: sin respuesta -> tiempo agotado",
          director::sacarResultado(resultado, nivel) &&
              resultado == simon::Resultado::TiempoAgotado && nivel == 2);
  revisar("simon: al segundo fallo seguido baja un nivel", simon::nivel() == 1);
  revisar("simon: cuenta 1 acierto y 2 errores", simon::aciertos() == 1 && simon::errores() == 2);

  uint16_t luz_fin = 0;
  bool crecio = true;
  for (uint8_t n = 1; n <= config::SIMON_NIVEL_MAX; ++n) {
    esperarTurno(t, vivo, luces);
    crecio = crecio && simon::nivel() == n && luces == n && simon::largo() == n;
    responderGatito(t, vivo, true);
    director::sacarResultado(resultado, nivel);
    crecio = crecio && resultado == simon::Resultado::Acierto;
  }
  revisar("simon: la secuencia crece de 1 a 6", crecio);
  while (!director::sacarLuz(luz)) {
    correr(t, t + 1, vivo);
  }
  luz_fin = luz.duracion_ms;
  revisar("simon: en el ultimo nivel sigue en 6 y la luz es mas corta",
          simon::nivel() == config::SIMON_NIVEL_MAX && luz_fin == config::SIMON_LUZ_MIN_MS);

  // Controles del terapeuta.
  simon::cambiarNivel(-10, t);
  revisar("simon: cambiar nivel no se sale de 1..6", simon::nivel() == config::SIMON_NIVEL_MAX);
  revisar("simon: bajar nivel a mano", simon::cambiarNivel(-4, t) == 2);
  esperarTurno(t, vivo, luces);
  revisar("simon: y muestra esa cantidad de pasos", luces == 2);
  revisar("simon: pista en el turno del nino", simon::pista());
  correr(t, t + 5, vivo);
  revisar("simon: la pista prende solo el paso que toca",
          director::sacarLuz(luz) && luz.led == simon::pasoDe(0).led);
  while (director::sacarReto(pedido, nivel)) {
  }
  simon::repetir(t);
  esperarTurno(t, vivo, luces);
  revisar("simon: repetir muestra todo sin contar otro turno",
          luces == 2 && !director::sacarReto(pedido, nivel));
  director::alternarSilencio(Nodo::Gatito);
  correr(t, t + 4000, vivo);
  director::sacarLuz(luz);
  correr(t, t + 4000, vivo);
  revisar("simon: a un animal silenciado no se le pide, y no es error",
          !director::sacarLuz(luz) && !director::sacarResultado(resultado, nivel));
  director::alternarSilencio(Nodo::Gatito);
  esperarTurno(t, vivo, luces);
  revisar("simon: al devolverlo sigue el juego", luces == 2);
  correr(t, t + 8000);  // sin latidos: se desconecta
  revisar("simon: si el animal se desconecta, no cuenta como error",
          simon::errores() <= 3 && !estado_animales::estaConectado(Nodo::Gatito, t));
}

// Gestos que vienen de antes no deben responder el turno siguiente.
void pruebasSimonGestos() {
  using interprete::Clase;
  simon::Resultado resultado;
  uint8_t nivel = 0;
  int luces = 0;
  {
    estado_animales::reiniciar();
    SintetizadorFalso s;
    uint32_t t = 1000;
    director::iniciar(s, 5, t, "prueba");
    director::cambiarModo(protocolo::Modo::Simon, t);
    const int vivo = protocolo::aEntero(Nodo::Ranita);
    // La ranita queda tapada desde antes y repite su nivel cada 500 ms.
    bool conto = false;
    for (int i = 0; i < 12; ++i) {
      director::alEvento(gesto(Nodo::Ranita, Clase::Sombra, 60), t);
      correr(t, t + 500, vivo);
      conto = conto || director::sacarResultado(resultado, nivel);
    }
    revisar("simon: ranita que ya estaba tapada no responde", !conto);
    simon::repetir(t);
    director::alEvento(gesto(Nodo::Ranita, Clase::Sombra, 0), t);
    esperarTurno(t, vivo, luces);
    s.limpiar();
    director::alEvento(gesto(Nodo::Ranita, Clase::Sombra, 20), t);
    revisar("simon: destaparla y volver a taparla si cuenta",
            director::sacarResultado(resultado, nivel) && resultado == simon::Resultado::Acierto);
    revisar("simon: la ranita suena igual que su pista",
            s.ultima_on[C_RANITA] == config::SIMON_NOTA_RANITA);
  }
  {
    estado_animales::reiniciar();
    SintetizadorFalso s;
    uint32_t t = 1000;
    director::iniciar(s, 5, t, "prueba");
    director::cambiarModo(protocolo::Modo::Simon, t);
    const int vivo = protocolo::aEntero(Nodo::Pollito);
    esperarTurno(t, vivo, luces);
    director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 20), t);
    revisar("simon: agitar el pollito -> acierto", director::sacarResultado(resultado, nivel) &&
                                                       resultado == simon::Resultado::Acierto);
    // Nivel 2: dos sacudidas separadas; el rebote a los 350 ms no es la segunda.
    esperarTurno(t, vivo, luces);
    director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 20), t);
    correr(t, t + 350, vivo);
    director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 20), t);
    revisar("simon: el rebote del pollito no cuenta", !director::sacarResultado(resultado, nivel));
    correr(t, t + 900, vivo);
    director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 20), t);
    revisar("simon: una sacudida nueva si cuenta", director::sacarResultado(resultado, nivel) &&
                                                       resultado == simon::Resultado::Acierto);
  }
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
  revisar("dj: A -> cancion", acciones_dj::deTecla('A', protocolo::Modo::Libre).tipo == Tipo::CambiarModo &&
                                  acciones_dj::deTecla('A', protocolo::Modo::Libre).modo == protocolo::Modo::Cancion);
  revisar("dj: B -> libre", acciones_dj::deTecla('B', protocolo::Modo::Libre).modo == protocolo::Modo::Libre);
  revisar("dj: C -> simon", acciones_dj::deTecla('C', protocolo::Modo::Libre).modo == protocolo::Modo::Simon);
  revisar("dj: 1/2/3 -> ranita/gatito/pollito",
          acciones_dj::deTecla('1', protocolo::Modo::Libre).nodo == Nodo::Ranita &&
              acciones_dj::deTecla('2', protocolo::Modo::Libre).nodo == Nodo::Gatito &&
              acciones_dj::deTecla('3', protocolo::Modo::Libre).tipo == Tipo::AlternarSilencio &&
              acciones_dj::deTecla('3', protocolo::Modo::Libre).nodo == Nodo::Pollito);
  revisar("dj: * graba, # play/stop, D deshace, D mantenida borra",
          acciones_dj::deTecla('*', protocolo::Modo::Libre).tipo == Tipo::GrabarLoop &&
              acciones_dj::deTecla('#', protocolo::Modo::Libre).tipo == Tipo::AlternarLoop &&
              acciones_dj::deTecla('D', protocolo::Modo::Libre).tipo == Tipo::DeshacerLoop &&
              acciones_dj::deTecla(acciones_dj::TECLA_D_LARGA, protocolo::Modo::Libre).tipo ==
                  Tipo::BorrarLoop);
  const auto simon_ = protocolo::Modo::Simon;
  revisar("dj: en Simon 7/9 nivel, * empezar, # repetir, 0 pista",
          acciones_dj::deTecla('7', simon_).tipo == Tipo::SimonNivel &&
              acciones_dj::deTecla('7', simon_).cuanto == -1 &&
              acciones_dj::deTecla('9', simon_).cuanto == 1 &&
              acciones_dj::deTecla('*', simon_).tipo == Tipo::SimonEmpezar &&
              acciones_dj::deTecla('#', simon_).tipo == Tipo::SimonRepetir &&
              acciones_dj::deTecla('0', simon_).tipo == Tipo::SimonPista);
  revisar("dj: en Simon 4 5 6 8 D no hacen nada y 1 A B si",
          acciones_dj::deTecla('4', simon_).tipo == Tipo::Ninguna &&
              acciones_dj::deTecla('8', simon_).tipo == Tipo::Ninguna &&
              acciones_dj::deTecla('D', simon_).tipo == Tipo::Ninguna &&
              acciones_dj::deTecla('1', simon_).tipo == Tipo::AlternarSilencio &&
              acciones_dj::deTecla('B', simon_).tipo == Tipo::CambiarModo);
  PulsacionLarga d(1000);
  d.iniciar(false);
  bool nada = d.actualizar(true, 0) == PulsacionLarga::Tipo::Nada &&
              d.actualizar(true, 500) == PulsacionLarga::Tipo::Nada;
  revisar("dj: D soltada antes de 1 s = corta",
          nada && d.actualizar(false, 600) == PulsacionLarga::Tipo::Corta);
  d.actualizar(true, 2000);
  revisar("dj: D a 1 s = larga, y al soltar no cuenta otra vez",
          d.actualizar(true, 3000) == PulsacionLarga::Tipo::Larga &&
              d.actualizar(true, 4000) == PulsacionLarga::Tipo::Nada &&
              d.actualizar(false, 4100) == PulsacionLarga::Tipo::Nada);
  revisar("dj: 7 baja y 9 sube el tempo", acciones_dj::deTecla('7', protocolo::Modo::Libre).cuanto < 0 &&
                                              acciones_dj::deTecla('9', protocolo::Modo::Libre).cuanto > 0);
  revisar("dj: 4 eco, 5 redoble, 6 subida, 8 ritmo, 0 acordes",
          acciones_dj::deTecla('4', protocolo::Modo::Libre).tipo == Tipo::Eco &&
              acciones_dj::deTecla('5', protocolo::Modo::Libre).tipo == Tipo::Redoble &&
              acciones_dj::deTecla('6', protocolo::Modo::Libre).tipo == Tipo::Subida &&
              acciones_dj::deTecla('8', protocolo::Modo::Libre).tipo == Tipo::Ritmo &&
              acciones_dj::deTecla('0', protocolo::Modo::Libre).tipo == Tipo::Acordes);

  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 7, t, "prueba");
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
  director::iniciar(s, 7, t, "prueba");
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
  director::iniciar(s, 7, t, "prueba");
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
  director::iniciar(s, 7, t, "prueba");
  director::cambiarModo(protocolo::Modo::Libre, t);
  s.limpiar();
  const uint32_t pulso = 250;

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

  // En Canción los efectos también suenan: van en canales que el fade no toca.
  director::cambiarModo(protocolo::Modo::Cancion, t);
  s.limpiar();
  director::redoble(t);
  correr(t, t + pulso * 4 + 20);
  revisar("efectos: el redoble suena tambien en Cancion", s.on[16] >= 8);
}

void pruebasDeshacer() {
  using interprete::Clase;
  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 7, t, "prueba");
  director::cambiarModo(protocolo::Modo::Libre, t);
  const uint32_t vuelta = 250 * 16;
  revisar("deshacer: sin loop no hay nada que quitar", !director::deshacerLoop());
  director::grabarLoop();
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 2, 90), t);
  director::alEvento(gesto(Nodo::Gatito, Clase::Nota, 2, 0), t);
  correr(t, t + vuelta + 300);
  director::grabarLoop();
  director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 20), t);
  correr(t, t + vuelta + 10);
  director::grabarLoop();  // una capa en la que no se toca nada
  correr(t, t + vuelta + 10);
  revisar("deshacer: quita la ultima capa con notas", director::deshacerLoop());
  s.limpiar();
  correr(t, t + vuelta);
  revisar("deshacer: queda la primera capa", s.on[C_GATITO] == 1 && s.on[C_POLLITO] == 0);
  director::cambiarModo(protocolo::Modo::Simon, t);
  s.limpiar();
  correr(t, t + vuelta);
  revisar("deshacer: en Simon el loop calla", s.on[C_GATITO] == 0);
  director::cambiarModo(protocolo::Modo::Libre, t);
  s.limpiar();
  correr(t, t + vuelta + 10);
  revisar("deshacer: al volver a Libre sigue", s.on[C_GATITO] == 1);
  revisar("deshacer: quita la primera", director::deshacerLoop() && !director::alternarLoop());
}

void pruebasBanda() {
  using acompanamiento::Ritmo;
  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 7, t, "prueba");
  director::cambiarModo(protocolo::Modo::Libre, t);
  s.limpiar();
  correr(t, t + 3 * 250 + 10);  // tres pulsos dentro del compás
  revisar("banda: 8 pide el primer ritmo", director::siguienteRitmo() == Ritmo::Tranquilo);
  revisar("banda: 0 pide acordes", director::alternarAcordes());
  correr(t, t + 4 * 250);
  revisar("banda: espera al siguiente compas", s.on[16] == 0 && s.on[14] == 0);
  correr(t, t + 2 * 250);
  revisar("banda: entra al compas con bombo y acorde de do", s.on[16] >= 1 && s.on[14] == 3);
  correr(t, t + 8 * 250);
  revisar("banda: el acorde cambia y se suelta el anterior",
          s.on[14] == 6 && s.off[14] == 3 && s.ultima_on[14] == 64);
  int distintos = 0;
  for (int i = 0; i < 3; ++i) {
    director::siguienteRitmo();
    correr(t, t + 8 * 250);  // que entre en el compás
    s.limpiar();
    correr(t, t + 16 * 250);
    distintos = distintos * 100 + s.on[16];
  }
  revisar("banda: pop y cumbia suenan distinto, y luego se apaga",
          distintos / 10000 > 0 && distintos / 100 % 100 > 0 &&
              distintos / 10000 != distintos / 100 % 100 && s.on[16] == 0);
  director::siguienteRitmo();
  correr(t, t + 8 * 250);
  director::alternarEco();
  director::cambiarModo(protocolo::Modo::Simon, t);
  revisar("banda: al entrar a Simon suelta el acorde", s.off[14] >= 3);
  s.limpiar();
  correr(t, t + 16 * 250);
  revisar("banda: callada en Simon", s.on[16] == 0 && s.on[14] == 0);
  director::cambiarModo(protocolo::Modo::Libre, t);
  s.limpiar();
  correr(t, t + 16 * 250);
  revisar("banda: al volver a Libre sigue", s.on[16] > 0 && s.on[14] > 0);
  director::cambiarTempo(100);
  s.limpiar();
  correr(t, t + 16 * 250);
  revisar("banda: sigue el tempo (200 % = el doble de acordes)", s.on[14] == 12);
}

void pruebasTempo() {
  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 7, t, "prueba");
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

void pruebasCancionRecibida() {
  namespace cr = cancion_recibida;
  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 7, t);
  revisar("recibida: al arrancar esta estrellita", strcmp(director::cancion(), "estrellita") == 0);
  const uint32_t unos[] = {1, 1, 2, 1};
  const auto cargar = [&](int32_t carga, const char* nombre, const char* melodia,
                          const uint32_t(&totales)[4]) {
    return cr::empezar(carga, nombre, 250, director::cancionEnUso()) &&
           cr::agregar(carga, 0, 0, "48.4.0") && cr::agregar(carga, 1, 0, "64.4.0") &&
           cr::agregar(carga, 2, 0, melodia) && cr::agregar(carga, 3, 0, "70.1.110") &&
           cr::terminar(carga, totales);
  };
  revisar("recibida: completa se puede elegir", cargar(1, "m-uno", "60.2.0,62.2.0", unos) &&
                                                    director::elegirCancion("m-uno"));
  s.limpiar();
  correr(t, t + 1200);
  revisar("recibida: y suena su base", s.on[C_OSITO] >= 1 && s.ultima_on[C_OSITO] == 48);

  revisar("recibida: totales que no cuadran se descartan", [&] {
    const uint32_t mas[] = {1, 1, 3, 1};
    return !cargar(2, "m-dos", "60.2.0,62.2.0", mas) && !director::elegirCancion("m-dos");
  }());
  revisar("recibida: sigue sonando la anterior", strcmp(director::cancion(), "m-uno") == 0);
  cr::empezar(3, "m-tres", 250, director::cancionEnUso());
  revisar("recibida: trozo perdido se descarta", cr::agregar(3, 2, 0, "60.2.0") &&
                                                     !cr::agregar(3, 2, 5, "60.2.0") &&
                                                     !cr::agregar(3, 0, 0, "48.4.0"));
  cr::empezar(4, "m-cuatro", 250, director::cancionEnUso());
  revisar("recibida: repetido se ignora, de otra carga no entra",
          cr::agregar(4, 2, 0, "60.2.0") && cr::agregar(4, 2, 0, "60.2.0") &&
              !cr::agregar(99, 0, 0, "48.4.0") && cr::agregar(4, 0, 0, "48.4.0") &&
              cr::agregar(4, 1, 0, "64.4.0") && cr::agregar(4, 2, 1, "62.2.0") &&
              cr::agregar(4, 3, 0, "70.1.1") && cr::terminar(4, unos));
  bool todos_rechazados = true;
  for (const char* malo : {"", "x", "60", "60.2", "60.2.", "60.0.0", "60.33.0", "128.1.0", "60.1.128",
                           "60.2.0,", "60.2.0;61.2.0", "-1.2.0", "60.2.0 ", "999999999999.1.1"}) {
    cr::empezar(5, "m-cinco", 250, director::cancionEnUso());
    todos_rechazados = todos_rechazados && !cr::agregar(5, 2, 0, malo);
  }
  revisar("recibida: pasos mal formados se rechazan", todos_rechazados);
  revisar("recibida: nombres y pulsos fuera de lugar se rechazan",
          !cr::empezar(6, "", 250, nullptr) && !cr::empezar(6, "Con Espacio", 250, nullptr) &&
              !cr::empezar(6, "m-demasiado-largo-para-24", 250, nullptr) &&
              !cr::empezar(6, "m-x", 10, nullptr) && !cr::empezar(6, "m-x", 5000, nullptr));
  cr::empezar(7, "m-siete", 250, director::cancionEnUso());
  bool cabe = true;
  for (uint32_t i = 0; i < config::CANCION_PASOS_MAX; ++i) {
    cabe = cabe && cr::agregar(7, 2, i, "60.1.0");
  }
  revisar("recibida: caben 128 pasos y ni uno mas",
          cabe && !cr::agregar(7, 2, config::CANCION_PASOS_MAX, "60.1.0"));

  // Varias seguidas sin elegirlas: nunca se escribe sobre la que suena.
  const canciones::Cancion* sonando = director::cancionEnUso();
  for (int32_t c = 10; c < 14; ++c) {
    cargar(c, "m-otra", "72.2.0,74.2.0", unos);
  }
  revisar("recibida: la que suena no cambia", director::cancionEnUso() == sonando &&
                                                  strcmp(director::cancion(), "m-uno") == 0 &&
                                                  sonando->partes[2].pasos[0].nota_midi == 60);
  cargar(20, "m-uno", "65.2.0,67.2.0", unos);
  revisar("recibida: editada, al elegirla entra la nueva",
          director::elegirCancion("m-uno") &&
              director::cancionEnUso()->partes[2].pasos[0].nota_midi == 65);

  contrato_mqtt::TrozoCancion trozo;
  using Tipo = contrato_mqtt::TrozoCancion::Tipo;
  revisar("recibida: lee inicio, pasos y fin",
          contrato_mqtt::leerTrozoCancion("{\"carga\":5,\"nombre\":\"m-a\",\"pulso\":250}", trozo) &&
              trozo.tipo == Tipo::Inicio && trozo.pulso_ms == 250 &&
              contrato_mqtt::leerTrozoCancion(
                  "{\"carga\":5,\"parte\":2,\"desde\":8,\"pasos\":\"60.2.0\"}", trozo) &&
              trozo.tipo == Tipo::Pasos && trozo.parte == 2 && trozo.desde == 8 &&
              contrato_mqtt::leerTrozoCancion("{\"carga\":5,\"totales\":\"1,2,3,4\"}", trozo) &&
              trozo.tipo == Tipo::Fin && trozo.totales[3] == 4);
  bool rechaza = true;
  for (const char* malo : {"{}", "{\"carga\":0,\"totales\":\"1,2,3,4\"}", "{\"carga\":5}",
                           "{\"carga\":5,\"totales\":\"1,2,3\"}", "{\"carga\":5,\"totales\":\"1,2,3,4,5\"}",
                           "{\"carga\":5,\"parte\":4,\"desde\":0,\"pasos\":\"60.2.0\"}",
                           "{\"carga\":5,\"nombre\":\"m-a\"}"}) {
    rechaza = rechaza && !contrato_mqtt::leerTrozoCancion(malo, trozo);
  }
  revisar("recibida: trozos invalidos se rechazan", rechaza);
}

void pruebasPanel() {
  estado_animales::reiniciar();
  SintetizadorFalso s;
  uint32_t t = 1000;
  director::iniciar(s, 7, t, "prueba");
  char json[config::EVENTO_JSON_MAX];
  revisar("panel: estado inicial",
          panel_json::armar(director::panel(), json, sizeof(json)) &&
              strcmp(json, "{\"tempo\":100,\"eco\":false,\"ritmo\":\"apagado\",\"acordes\":false,"
                           "\"loop\":\"vacio\",\"capas\":0,\"callados\":0,\"nivel\":1,\"avance\":0}") == 0);
  director::cambiarModo(protocolo::Modo::Libre, t);
  director::alternarEco();
  director::siguienteRitmo();
  director::alternarSilencio(Nodo::Pollito);
  director::cambiarTempo(10);
  director::grabarLoop();
  director::alEvento(gesto(Nodo::Gatito, interprete::Clase::Nota, 2, 90), t);
  panel_json::armar(director::panel(), json, sizeof(json));
  revisar("panel: refleja eco, ritmo, tempo, callados y grabacion",
          strstr(json, "\"tempo\":110,\"eco\":true,\"ritmo\":\"tranquilo\"") != nullptr &&
              strstr(json, "\"loop\":\"grabando\",\"capas\":1,\"callados\":8") != nullptr);
  director::Panel largo = {200, true, acompanamiento::Ritmo::Tranquilo, true,
                           director::EstadoLoop::Grabando, 255, 255, 255, 100};
  revisar("panel: el mas largo cabe", panel_json::armar(largo, json, sizeof(json)));
  director::cambiarModo(protocolo::Modo::Cancion, t);
  correr(t, t + 800);
  const uint8_t mitad = director::panel().avance;
  revisar("panel: avance de la cancion", mitad <= 100);
  char tecla = 0;
  revisar("panel: teclas validas", contrato_mqtt::leerTecla("{\"tecla\":\"#\"}", tecla) &&
                                       tecla == '#' &&
                                       contrato_mqtt::leerTecla("{\"tecla\":\"d\"}", tecla));
  revisar("panel: teclas invalidas se rechazan", !contrato_mqtt::leerTecla("{\"tecla\":\"z\"}", tecla) &&
                                                     !contrato_mqtt::leerTecla("{\"tecla\":\"\"}", tecla) &&
                                                     !contrato_mqtt::leerTecla("{\"tecla\":\"12\"}", tecla) &&
                                                     !contrato_mqtt::leerTecla("{}", tecla));
  // Todas las incluidas: existen, duran algo y sus partes suenan.
  bool todas = true;
  for (const canciones::Cancion& c : canciones::INCLUIDAS) {
    todas = todas && director::elegirCancion(c.nombre);
    s.limpiar();
    correr(t, t + 6000);
    todas = todas && s.on[C_OSITO] > 0;
  }
  revisar("canciones: las 13 incluidas se eligen y suenan",
          todas && std::size(canciones::INCLUIDAS) == 13);
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
          contrato_mqtt::armarEventoSimon(10, 0, Nodo::Ranita, simon::Resultado::Nada, 2, buf,
                                          sizeof(buf)) &&
              strstr(buf, "\"pedido\":\"ranita\",\"nivel\":2}") != nullptr);
  revisar("mqtt: simon tiempo agotado se llama \"tiempo\"",
          contrato_mqtt::armarEventoSimon(11, 0, Nodo::Pollito, simon::Resultado::TiempoAgotado, 3,
                                          buf, sizeof(buf)) &&
              strstr(buf, "\"pedido\":\"pollito\",\"nivel\":3,\"resultado\":\"tiempo\"}") != nullptr);
  revisar("mqtt: volumen", contrato_mqtt::armarVolumen(70, buf, sizeof(buf)) &&
                               strcmp(buf, "{\"valor\":70}") == 0);
  revisar("mqtt: estado de la cola", contrato_mqtt::armarCola(37, 2, buf, sizeof(buf)) &&
                                         strcmp(buf, "{\"pendientes\":37,\"descartados\":2}") == 0);
  strcpy(buf, "{\"seq\":1}");
  revisar("mqtt: agrega el arranque al final",
          contrato_mqtt::agregarArranque(buf, sizeof(buf), 42) &&
              strcmp(buf, "{\"seq\":1,\"arranque\":42}") == 0);
  char justo[12] = "{\"seq\":1}";
  revisar("mqtt: si no cabe el arranque avisa y no escribe de mas",
          !contrato_mqtt::agregarArranque(justo, sizeof(justo), 42) &&
              strcmp(justo, "{\"seq\":1}") == 0);
  revisar("mqtt: el evento mas largo cabe con arranque",
          contrato_mqtt::armarEventoModo(4294967295u, 4294967295u, protocolo::Modo::Cancion,
                                         "xxxxxxxxxxxxxxxxxxxxxxx", contrato_mqtt::Origen::NodeRed,
                                         buf, sizeof(buf)) &&
              contrato_mqtt::agregarArranque(buf, sizeof(buf), 999999));
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
  pruebasSimonGestos();
  pruebasModos();
  pruebasDj();
  pruebasLooper();
  pruebasLooperCancelar();
  pruebasTempo();
  pruebasEfectos();
  pruebasDeshacer();
  pruebasCancionRecibida();
  pruebasPanel();
  pruebasBanda();
  pruebasContratoMqtt();
  pruebasColaEventos();
  printf("[SIMULACIÓN] %d de %d pruebas pasaron\n", pruebas - fallos, pruebas);
  return fallos == 0 ? 0 : 1;
}
