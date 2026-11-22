// AES-128-GCM con mbedtls (viene en el core del ESP32).
#include "cifrado.h"

#include <esp_random.h>
#include <mbedtls/gcm.h>

namespace cifrado {
namespace {
constexpr unsigned BITS_CLAVE = CLAVE_BYTES * 8;

// Libera el contexto de mbedtls aunque se salga antes por un error.
class Contexto {
 public:
  Contexto() {
    mbedtls_gcm_init(&gcm);
  }
  ~Contexto() {
    mbedtls_gcm_free(&gcm);
  }
  mbedtls_gcm_context gcm;
};
}  // namespace

bool cifrar(const uint8_t* clave, const uint8_t* aad, size_t tam_aad, const uint8_t* entrada,
            size_t tamano, uint8_t* nonce, uint8_t* salida, uint8_t* tag) {
  // Con la radio encendida, esp_fill_random da números aleatorios de verdad.
  esp_fill_random(nonce, NONCE_BYTES);
  Contexto contexto;
  return mbedtls_gcm_setkey(&contexto.gcm, MBEDTLS_CIPHER_ID_AES, clave, BITS_CLAVE) == 0 &&
         mbedtls_gcm_crypt_and_tag(&contexto.gcm, MBEDTLS_GCM_ENCRYPT, tamano, nonce, NONCE_BYTES,
                                   aad, tam_aad, entrada, salida, TAG_BYTES, tag) == 0;
}

bool descifrar(const uint8_t* clave, const uint8_t* aad, size_t tam_aad, const uint8_t* nonce,
               const uint8_t* entrada, size_t tamano, const uint8_t* tag, uint8_t* salida) {
  Contexto contexto;
  return mbedtls_gcm_setkey(&contexto.gcm, MBEDTLS_CIPHER_ID_AES, clave, BITS_CLAVE) == 0 &&
         mbedtls_gcm_auth_decrypt(&contexto.gcm, tamano, nonce, NONCE_BYTES, aad, tam_aad, tag,
                                  TAG_BYTES, entrada, salida) == 0;
}
}  // namespace cifrado
