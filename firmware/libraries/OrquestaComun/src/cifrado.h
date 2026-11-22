// AES-128-GCM con nonce aleatorio de 12 bytes, el mismo esquema de la Práctica 4.
// Lo usa el enlace ESP-NOW y lo puede usar el osito para los eventos MQTT.
#pragma once

#include <cstddef>
#include <cstdint>

namespace cifrado {
constexpr size_t CLAVE_BYTES = 16;
constexpr size_t NONCE_BYTES = 12;
constexpr size_t TAG_BYTES = 16;

// Genera el nonce, cifra `entrada` en `salida` y calcula el tag.
// `aad` no se cifra pero sí queda autenticada: si alguien la cambia, el tag ya no cuadra.
bool cifrar(const uint8_t* clave, const uint8_t* aad, size_t tam_aad, const uint8_t* entrada,
            size_t tamano, uint8_t* nonce, uint8_t* salida, uint8_t* tag);

// false si el tag no cuadra: se alteró el mensaje o la aad, o la clave es otra.
bool descifrar(const uint8_t* clave, const uint8_t* aad, size_t tam_aad, const uint8_t* nonce,
               const uint8_t* entrada, size_t tamano, const uint8_t* tag, uint8_t* salida);
}  // namespace cifrado
