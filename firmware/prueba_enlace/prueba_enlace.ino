// Prueba del enlace con dos placas ya aprovisionadas: una como osito y otra como animal.
// El animal manda latidos y una sombra cada segundo; el osito imprime lo que recibe, dice
// quién está conectado y cada pocos segundos manda un cambio de modo a todos.
// Al arrancar también revisa que un mensaje alterado o repetido se rechace (evidencia
// para la matriz de pruebas).
#include <cifrado.h>
#include <contra_replay.h>
#include <enlace.h>
#include <protocolo.h>

// Cambiar a Nodo::Osito en la placa que hace de osito.
constexpr protocolo::Nodo YO = protocolo::Nodo::Ranita;

namespace {
using protocolo::Nodo;

constexpr uint32_t BAUDIOS = 115200;
constexpr uint32_t ENVIO_CADA_MS = 1000;
constexpr uint32_t MODO_CADA_MS = 5000;
constexpr uint32_t ESTADO_CADA_MS = 2000;

uint32_t ultimo_envio_ms = 0;
uint32_t ultimo_modo_ms = 0;
uint32_t ultimo_estado_ms = 0;
uint8_t nivel = 0;
uint8_t modo = 0;

void resultado(const char* prueba, bool paso) {
  Serial.printf("[autoprueba] %-36s %s\n", prueba, paso ? "OK" : "FALLO");
}

void autoprueba() {
  const uint8_t clave[cifrado::CLAVE_BYTES] = {1, 2,  3,  4,  5,  6,  7,  8,
                                               9, 10, 11, 12, 13, 14, 15, 16};
  const uint8_t aad[] = {0xAA, 0xBB};
  const uint8_t claro[] = {42, 7};
  uint8_t nonce[cifrado::NONCE_BYTES];
  uint8_t secreto[sizeof(claro)];
  uint8_t tag[cifrado::TAG_BYTES];
  uint8_t salida[sizeof(claro)];

  cifrado::cifrar(clave, aad, sizeof(aad), claro, sizeof(claro), nonce, secreto, tag);
  const bool descifra =
      cifrado::descifrar(clave, aad, sizeof(aad), nonce, secreto, sizeof(claro), tag, salida) &&
      memcmp(salida, claro, sizeof(claro)) == 0;
  resultado("descifra un mensaje correcto", descifra);

  secreto[0] ^= 1;
  resultado("rechaza payload alterado", !cifrado::descifrar(clave, aad, sizeof(aad), nonce, secreto,
                                                            sizeof(claro), tag, salida));
  secreto[0] ^= 1;

  const uint8_t aad_alterada[] = {0xAA, 0xBC};
  resultado("rechaza cabecera alterada",
            !cifrado::descifrar(clave, aad_alterada, sizeof(aad_alterada), nonce, secreto,
                                sizeof(claro), tag, salida));

  ContraReplay contra_replay;
  const uint8_t ranita = protocolo::aEntero(Nodo::Ranita);
  const bool acepta = contra_replay.aceptar(ranita, 1, 5) && contra_replay.aceptar(ranita, 1, 6);
  resultado("acepta seq que sube", acepta);
  resultado("rechaza seq repetido", !contra_replay.aceptar(ranita, 1, 6));
  resultado("rechaza seq viejo", !contra_replay.aceptar(ranita, 1, 3));
  resultado("acepta seq 0 tras reinicio del emisor", contra_replay.aceptar(ranita, 2, 0));
}

void imprimir(const enlace::Mensaje& mensaje) {
  const char* de = protocolo::NOMBRE_NODO[mensaje.cabecera.nodo_emisor];
  protocolo::PayloadSombra sombra;
  protocolo::PayloadModo payload_modo;
  if (mensaje.tipo() == protocolo::TipoMensaje::Latido) {
    Serial.printf("<- %s latido (seq %lu)\n", de, static_cast<unsigned long>(mensaje.cabecera.seq));
  } else if (mensaje.leer(sombra)) {
    Serial.printf("<- %s sombra nivel=%u\n", de, sombra.nivel);
  } else if (mensaje.leer(payload_modo) && payload_modo.modo < protocolo::NUM_MODOS) {
    Serial.printf("<- %s modo=%s\n", de, protocolo::NOMBRE_MODO[payload_modo.modo]);
  } else {
    Serial.printf("<- %s tipo %u\n", de, mensaje.cabecera.tipo);
  }
}
}  // namespace

void setup() {
  Serial.begin(BAUDIOS);
  autoprueba();
  if (!enlace::iniciar(YO)) {
    Serial.println("El enlace no arrancó: revisa el aprovisionamiento.");
  }
}

void loop() {
  enlace::actualizar();

  enlace::Mensaje mensaje;
  while (enlace::recibir(mensaje)) {
    imprimir(mensaje);
  }

  const uint32_t ahora_ms = millis();
  if (YO != Nodo::Osito && ahora_ms - ultimo_envio_ms >= ENVIO_CADA_MS) {
    ultimo_envio_ms = ahora_ms;
    nivel = (nivel + 10) % (protocolo::NIVEL_SOMBRA_MAX + 1);
    enlace::enviar(protocolo::PayloadSombra{nivel});
  }

  if (YO == Nodo::Osito && ahora_ms - ultimo_modo_ms >= MODO_CADA_MS) {
    ultimo_modo_ms = ahora_ms;
    modo = (modo + 1) % protocolo::NUM_MODOS;
    enlace::enviar(protocolo::PayloadModo{modo});
    Serial.printf("-> todos modo=%s\n", protocolo::NOMBRE_MODO[modo]);
  }

  if (YO == Nodo::Osito && ahora_ms - ultimo_estado_ms >= ESTADO_CADA_MS) {
    ultimo_estado_ms = ahora_ms;
    for (size_t i = 0; i < protocolo::NUM_NODOS; ++i) {
      if (static_cast<Nodo>(i) != Nodo::Osito) {
        Serial.printf("   %s: %s\n", protocolo::NOMBRE_NODO[i],
                      enlace::estaConectado(static_cast<Nodo>(i)) ? "conectado" : "desconectado");
      }
    }
  }
}
