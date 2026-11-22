// Pruebas en la compu de la lógica pura del osito (sin placa, sin radio). Correr con ./correr.sh.
// Todos los mensajes son SIMULACIÓN armados a mano: esto no demuestra que la radio funcione.
#include <cstdio>
#include <cstring>

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
  void notaOn(uint8_t canal, uint8_t nota, uint8_t) override {
    ++on[canal];
    ultima_on[canal] = nota;
  }
  void notaOff(uint8_t canal, uint8_t) override {
    ++off[canal];
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

  director::alEvento(gesto(Nodo::Pollito, Clase::Agitar, 70), t);
  revisar("libre: pollito agitado -> golpe en canal 10",
          s.on[C_POLLITO] == 1 && s.ultima_on[C_POLLITO] == 70);
  correr(t, t + 150);
  revisar("libre: el golpe se apaga solo", s.off[C_POLLITO] == 1);

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
  pruebasContratoMqtt();
  pruebasColaEventos();
  printf("[SIMULACIÓN] %d de %d pruebas pasaron\n", pruebas - fallos, pruebas);
  return fallos == 0 ? 0 : 1;
}
