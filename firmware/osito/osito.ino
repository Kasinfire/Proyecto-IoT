// Osito: gateway y director musical. Recibe a los animales por ESP-NOW, decide qué suena
// y (cuando esté configurado) reporta al broker. Aquí solo se llaman los módulos.
#include <esp_random.h>
#include <protocolo.h>

#include "acciones_dj.h"
#include "animales.h"
#include "canciones.h"
#include "config.h"
#include "director.h"
#include "entradas.h"
#include "estado_animales.h"
#include "luces.h"
#include "nube.h"
#include "panel_json.h"
#include "simulacion.h"
#include "teclado_dj.h"
#include "sintetizador_registro.h"
#include "transporte_mqtt_real.h"
#include "transporte_simulado.h"

namespace {
// El sonido sale por la laptop: el osito imprime cada nota por serie y la laptop la toca.
SintetizadorRegistro sintetizador;
// La conexión real usa TLS validado por la CA del proyecto. El simulador queda aislado para
// probar comandos; sus mensajes no se publican en el broker real.
TransporteMqttReal broker;
TransporteSimulado broker_simulado;
bool inicio_radio_intentado = false;
uint32_t arranque_ms = 0;
protocolo::Nodo pedido_simon = protocolo::Nodo::Osito;  // primer animal del reto de Simón
bool reto_simulado = false;

void anunciarModo() {
  const protocolo::Modo modo = director::modo();
  luces::mostrarModo(modo);
  animales::avisarModo(modo);
  Serial.printf("[osito] modo=%s cancion=%s\n", protocolo::NOMBRE_MODO[protocolo::aEntero(modo)],
                director::cancion());
}

// Cada cambio de modo queda en el historial, con quién lo pidió (también botón y teclado).
void registrarModo(contrato_mqtt::Origen origen, bool simulado) {
  nube::alModo(director::modo(), director::cancion(), origen, simulado);
}

// Un reto con un animal que solo existe en la simulación no va al broker real.
bool secuenciaSimulada(uint32_t ahora_ms) {
  for (size_t i = 0; i < simon::largo(); ++i) {
    if (!estado_animales::estaConectado(simon::pasoDe(i).nodo, estado_animales::Fuente::Radio,
                                        ahora_ms)) {
      return true;
    }
  }
  return false;
}

// El tablero sabe si el osito aceptó el cambio con este acuse.
void atenderPedidosModo(uint32_t ahora_ms) {
  contrato_mqtt::PedidoModo pedido;
  while (nube::sacarPedidoModo(pedido)) {
    const bool cancion_existe = !pedido.trae_cancion || canciones::buscar(pedido.cancion);
    const bool ok = pedido.modo_valido && cancion_existe;
    if (ok) {
      if (pedido.trae_cancion) {
        director::elegirCancion(pedido.cancion);
      }
      director::cambiarModo(pedido.modo, ahora_ms);
      anunciarModo();
      registrarModo(contrato_mqtt::Origen::NodeRed, pedido.simulado);
    } else {
      Serial.printf("[osito] Pedido de modo %ld rechazado (%s)\n", static_cast<long>(pedido.id),
                    pedido.modo_valido ? "esa canción no existe" : "modo desconocido");
    }
    nube::responderModo(pedido.id, ok, director::modo(), pedido.simulado);
  }
}

// Canción creada en la página: se arma aparte y solo se puede elegir si llegó completa.
void atenderCancion() {
  contrato_mqtt::TrozoCancion trozo;
  while (nube::sacarTrozoCancion(trozo)) {
    bool ok = false;
    switch (trozo.tipo) {
      case contrato_mqtt::TrozoCancion::Tipo::Inicio:
        ok = cancion_recibida::empezar(trozo.carga, trozo.nombre, trozo.pulso_ms,
                                       director::cancionEnUso());
        break;
      case contrato_mqtt::TrozoCancion::Tipo::Pasos:
        ok = cancion_recibida::agregar(trozo.carga, trozo.parte, trozo.desde, trozo.pasos);
        break;
      case contrato_mqtt::TrozoCancion::Tipo::Fin:
        ok = cancion_recibida::terminar(trozo.carga, trozo.totales);
        Serial.printf("[osito] Canción de la página: %s\n",
                      ok ? "completa" : "incompleta, se descarta");
        nube::responderCancion(trozo.carga, ok);
        break;
    }
  }
}

// Teclado DJ: A/B/C escogen el modo; 1/2/3 silencian o devuelven a ranita/gatito/pollito.
void atenderTeclado(uint32_t ahora_ms) {
  char tecla;
  teclado_dj::Origen origen = teclado_dj::Origen::Fisica;
  while (nube::sacarTeclaRemota(tecla)) {
    teclado_dj::meterRemota(tecla);
  }
  while (teclado_dj::siguienteTecla(tecla, origen)) {
    const bool simulada = origen == teclado_dj::Origen::Simulada;
    if (!simulada) {
      nube::alTecla(tecla);  // la página la ilumina, venga del teclado o de la pantalla
    }
    const acciones_dj::Accion accion = acciones_dj::deTecla(tecla, director::modo());
    switch (accion.tipo) {
      case acciones_dj::Tipo::CambiarModo:
        Serial.printf("[dj] tecla %c\n", tecla);
        director::cambiarModo(accion.modo, ahora_ms);
        anunciarModo();
        registrarModo(contrato_mqtt::Origen::Teclado, simulada);
        break;
      case acciones_dj::Tipo::AlternarSilencio: {
        const bool callado = director::alternarSilencio(accion.nodo);
        Serial.printf("[dj] tecla %c: %s %s\n", tecla,
                      protocolo::NOMBRE_NODO[protocolo::aEntero(accion.nodo)],
                      callado ? "silenciado" : "suena otra vez");
        break;
      }
      case acciones_dj::Tipo::GrabarLoop:
        if (director::modo() == protocolo::Modo::Cancion) {
          Serial.println("[dj] tecla *: el loop se graba en Libre (tecla B)");
          break;
        }
        director::grabarLoop();
        Serial.println("[dj] tecla *: grabando loop (2 compases, suena el metrónomo)");
        break;
      case acciones_dj::Tipo::AlternarLoop:
        Serial.printf("[dj] tecla #: loop %s\n", director::alternarLoop() ? "sonando" : "parado");
        break;
      case acciones_dj::Tipo::DeshacerLoop:
        Serial.printf("[dj] tecla D: %s\n",
                      director::deshacerLoop() ? "última capa quitada" : "no hay capas que quitar");
        break;
      case acciones_dj::Tipo::BorrarLoop:
        director::borrarLoop();
        Serial.println("[dj] tecla D mantenida: loop borrado");
        break;
      case acciones_dj::Tipo::CambiarTempo:
        Serial.printf("[dj] tecla %c: tempo %u%%\n", tecla,
                      director::cambiarTempo(accion.cuanto));
        break;
      case acciones_dj::Tipo::Eco:
        Serial.printf("[dj] tecla 4: eco %s\n", director::alternarEco() ? "encendido" : "apagado");
        break;
      case acciones_dj::Tipo::Redoble:
        director::redoble(ahora_ms);
        Serial.println("[dj] tecla 5: redoble");
        break;
      case acciones_dj::Tipo::Subida:
        director::subida(ahora_ms);
        Serial.println("[dj] tecla 6: subida y caída");
        break;
      case acciones_dj::Tipo::Ritmo:
        Serial.printf("[dj] tecla 8: ritmo %s (entra en el siguiente compás)\n",
                      acompanamiento::NOMBRE_RITMO[static_cast<uint8_t>(director::siguienteRitmo())]);
        break;
      case acciones_dj::Tipo::Acordes:
        Serial.printf("[dj] tecla 0: acordes %s (en el siguiente compás)\n",
                      director::alternarAcordes() ? "encendidos" : "apagados");
        break;
      case acciones_dj::Tipo::SimonNivel:
        Serial.printf("[simon] tecla %c: nivel %u\n", tecla,
                      simon::cambiarNivel(accion.cuanto, ahora_ms));
        break;
      case acciones_dj::Tipo::SimonEmpezar:
        simon::empezar(ahora_ms);
        Serial.println("[simon] tecla *: empieza de nuevo");
        break;
      case acciones_dj::Tipo::SimonRepetir:
        simon::repetir(ahora_ms);
        Serial.println("[simon] tecla #: se repite el reto");
        break;
      case acciones_dj::Tipo::SimonPista:
        Serial.printf("[simon] tecla 0: %s\n",
                      simon::pista() ? "pista" : "la pista solo se da en el turno del niño");
        break;
      default:
        Serial.printf("[dj] tecla %c (sin uso)\n", tecla);
        break;
    }
  }
}
// ESP-NOW usa el canal del router, así que se arranca al asociarse al Wi-Fi. Plan B: sin
// credenciales de Wi-Fi o si el router no responde a tiempo, arranca en el canal por defecto
// para que la orquesta funcione igual (sin MQTT). Los animales oyen el modo hasta que la
// radio existe, por eso se anuncia otra vez.
void iniciarRadioCuandoToque() {
  if (inicio_radio_intentado) {
    return;
  }
  const char* motivo = nullptr;
  if (broker.wifiConectado()) {
    motivo = "Wi-Fi conectado: canal del router";
  } else if (!broker.tieneCredenciales()) {
    motivo = "sin datos de Wi-Fi en NVS: canal por defecto, sin MQTT";
  } else if (millis() - arranque_ms >= config::ESPERA_WIFI_PARA_RADIO_MS) {
    motivo = "el Wi-Fi no respondió a tiempo: canal actual; si conecta después, la radio pasa "
             "al canal del router";
  } else {
    return;
  }
  inicio_radio_intentado = true;
  Serial.printf("[osito] Iniciando ESP-NOW (%s)\n", motivo);
  animales::iniciar();
  anunciarModo();
}
}  // namespace

void setup() {
  Serial.begin(config::BAUDIOS);
  Serial.println("[osito] Arrancando");
  arranque_ms = millis();
  entradas::iniciar();
  teclado_dj::iniciar();
  luces::iniciar();
  sintetizador.iniciar();
  director::iniciar(sintetizador, esp_random(), millis());
  director::volumen(entradas::volumen());
  nube::iniciar(broker);  // inicia Wi-Fi; ESP-NOW se arranca después de asociarse al router
  broker_simulado.iniciar(nube::inyectarSimulado);
  simulacion::iniciar(broker_simulado);
  anunciarModo();
  registrarModo(contrato_mqtt::Origen::Arranque, false);
}

void loop() {
  simulacion::actualizar();
  entradas::actualizar();
  teclado_dj::actualizar();
  iniciarRadioCuandoToque();
  animales::actualizar();

  const uint32_t ahora_ms = millis();
  animales::Recibido recibido;
  while (animales::siguiente(recibido)) {
    director::alEvento(recibido.evento, ahora_ms);
    nube::alEvento(recibido);
  }
  estado_animales::CambioConexion cambio;
  while (animales::siguienteCambio(cambio)) {
    nube::alCambio(cambio);
    if (!cambio.conectado) {
      director::avisoDesconexion();
    }
  }

  bool presion_simulada = false;
  if (entradas::sePresionoModo(presion_simulada)) {
    director::siguienteModo(ahora_ms);
    anunciarModo();
    registrarModo(contrato_mqtt::Origen::Boton, presion_simulada);
  }
  bool silenciar = false;
  if (nube::sacarAviso(silenciar)) {
    director::silenciarAviso(silenciar);
    Serial.printf("[osito] Sonido de aviso %s desde Node-RED (el LED sigue)\n",
                  silenciar ? "silenciado" : "activado");
  }
  atenderTeclado(ahora_ms);
  size_t notas_en_loop = 0;
  if (director::sacarFinGrabacion(notas_en_loop)) {
    Serial.printf("[dj] loop listo: %u notas, ya se repite solo\n",
                  static_cast<unsigned>(notas_en_loop));
  }
  atenderCancion();
  atenderPedidosModo(ahora_ms);
  char panel[config::EVENTO_JSON_MAX];
  if (panel_json::armar(director::panel(), panel, sizeof(panel))) {
    nube::alPanel(panel);
  }
  if (entradas::huboCambioVolumen()) {
    director::volumen(entradas::volumen());
    nube::alVolumen(entradas::volumen());
  }

  director::actualizar(ahora_ms);
  sintetizador.actualizar();

  if (director::huboPulso()) {
    luces::pulso();
  }
  protocolo::PayloadLuz luz;
  if (director::sacarLuz(luz)) {
    animales::pedirLuz(luz);
  }
  uint8_t nivel = 1;
  if (director::sacarReto(pedido_simon, nivel)) {
    Serial.printf("[simon] nivel %u:", nivel);
    for (size_t i = 0; i < simon::largo(); ++i) {
      const simon::Paso paso = simon::pasoDe(i);
      Serial.printf(" %s%s%s", protocolo::NOMBRE_NODO[protocolo::aEntero(paso.nodo)],
                    paso.nodo == protocolo::Nodo::Gatito ? "-" : "",
                    paso.nodo == protocolo::Nodo::Gatito ? protocolo::NOMBRE_NOTA[paso.led] : "");
    }
    Serial.println();
    reto_simulado = secuenciaSimulada(ahora_ms);
    nube::alSimon(pedido_simon, simon::Resultado::Nada, nivel, reto_simulado);
  }
  simon::Resultado resultado;
  if (director::sacarResultado(resultado, nivel)) {
    Serial.printf("[simon] %s en nivel %u (aciertos=%u errores=%u)\n",
                  simon::NOMBRE_RESULTADO[protocolo::aEntero(resultado)], nivel,
                  simon::aciertos(), simon::errores());
    nube::alSimon(pedido_simon, resultado, nivel, reto_simulado);
  }

  nube::actualizar();
  luces::alerta(estado_animales::algunoSeDesconecto(ahora_ms));
  luces::actualizar();
}
