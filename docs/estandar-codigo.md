# Estándar de código

Reglas que seguimos todos en el firmware. Si algo no está aquí, se platica en el PR y se agrega.

## 1. Entorno

- **Arduino IDE 2** con el core **esp32 3.3.x** (Espressif). Todos la misma versión: la API de ESP-NOW cambió entre 2.x y 3.x.
- **Sketchbook location** (Archivo → Preferencias) apuntando a la carpeta `firmware/` del repo. Así el IDE encuentra la librería compartida `firmware/libraries/OrquestaComun` sin copiar nada.
  - Las librerías de terceros que instalen caerán en `firmware/libraries/` también; están en `.gitignore`.
- Formato automático: copiar `.clang-format` de la raíz del repo a la carpeta de configuración del IDE (`~/.arduinoIDE/` en Linux/Mac, `%USERPROFILE%\.arduinoIDE\` en Windows) y formatear con **Ctrl+T** antes de cada commit.

## 2. Estructura de un sketch

El nombre de la carpeta y del `.ino` deben coincidir (`firmware/ranita/ranita.ino`). Los `.h`/`.cpp` van junto al `.ino` (el IDE los muestra como pestañas).

```
firmware/ranita/
  ranita.ino          solo setup() y loop(), llamando a los módulos
  pines.h             números de GPIO
  config.h            umbrales, tiempos, rangos
  sensor_sombra.h/.cpp
  mejillas.h/.cpp
  enlace.h/.cpp       envío/recepción ESP-NOW (usa OrquestaComun)
```

- **Un módulo = un `.h` + un `.cpp` con una sola responsabilidad.** Si el nombre del módulo necesita un "y", son dos módulos.
- Cada módulo es un `namespace` con `iniciar()` (se llama en `setup()`) y `actualizar()` (se llama en cada `loop()`), más las funciones que necesite exponer.
- Lo que use más de un animal va en `OrquestaComun`, no copiado en cada sketch.

```cpp
// sensor_sombra.h
#pragma once
#include <cstdint>

namespace sensor_sombra {
void iniciar();
void actualizar();
uint8_t nivel();  // 0..protocolo::NIVEL_SOMBRA_MAX
}  // namespace sensor_sombra
```

```cpp
// ranita.ino
#include "enlace.h"
#include "mejillas.h"
#include "sensor_sombra.h"

void setup() {
  sensor_sombra::iniciar();
  mejillas::iniciar();
  enlace::iniciar();
}

void loop() {
  sensor_sombra::actualizar();
  mejillas::actualizar();
  enlace::actualizar();
}
```

## 3. Nada hardcodeado

- **Pines** solo en `pines.h`. **Umbrales, tiempos y rangos** solo en `config.h`. Ningún número suelto en la lógica (salvo 0 y 1 obvios).
- **Credenciales** (SSID, contraseñas, claves AES) nunca en el código: se leen de NVS, aprovisionadas con `herramientas/aprovisionar.py`.
- Constantes con `constexpr`, no `#define`. `#define` solo para `#pragma once`/compilación condicional.
- Enums con `enum class Nombre : uint8_t`, no enums planos.

```cpp
// pines.h
#pragma once
#include <cstdint>

namespace pines {
constexpr uint8_t LDR = 34;  // ADC1: ADC2 no funciona con Wi-Fi/ESP-NOW activos
constexpr uint8_t MEJILLA_IZQ = 25;
constexpr uint8_t MEJILLA_DER = 26;
}  // namespace pines
```

### Plantillas

Se usan cuando quitan código repetido o mueven un error de la demo al compilador:

- Utilidades genéricas que usan varios animales (buffers, filtros, antirrebote) van en `OrquestaComun` como plantillas.
- Siempre en un `.h`, nunca en el `.ino`: el generador de prototipos del IDE se confunde con ellas.
- Para convertir un `enum class` a número se usa `protocolo::aEntero(valor)`, no `static_cast` suelto.
- Si una plantilla necesita más de un comentario para entenderse, probablemente no vale la pena.

## 4. Nada bloqueante

- **Prohibido `delay()` en `loop()`** y dentro de los módulos. Todo con `millis()` y, si hay pasos, una máquina de estados (`enum class Estado` + `switch`).
- Los callbacks de ESP-NOW solo copian el mensaje a un buffer y salen; el procesamiento se hace en `actualizar()`.
- Nada de `while` esperando un evento.

## 5. Nombres

Español, sin acentos ni ñ en identificadores (`tamano`, no `tamaño`).

| Qué | Estilo | Ejemplo |
|---|---|---|
| Archivos | `snake_case` | `sensor_sombra.cpp` |
| Namespaces | `snake_case` | `sensor_sombra` |
| Tipos, structs, enums | `PascalCase` | `PayloadNota`, `TipoMensaje` |
| Valores de `enum class` | `PascalCase` | `Modo::Simon` |
| Funciones | `camelCase` | `leerNivel()` |
| Variables y campos | `snake_case` | `ultimo_envio_ms` |
| Constantes `constexpr` | `UPPER_SNAKE` | `LATIDO_CADA_MS` |

- Unidades en el nombre: `_ms`, `_hz`, `_pct`. Así no hace falta comentarlas.
- Booleanos como pregunta: `hay_sombra`, `esta_conectado`.

## 6. Variables globales

- Solo dentro de su módulo, en un `namespace` anónimo dentro del `.cpp`. Nunca `extern` entre módulos: si otro módulo necesita el dato, se expone una función.

## 7. Comentarios

El proyecto pide código "estructurado y comentado", así que se comenta, pero con criterio:

- Cada archivo empieza con 1–3 líneas: qué hace el módulo.
- Se comenta el **por qué**, no el **qué**. `// ADC1: ADC2 no funciona con Wi-Fi` sí; `// lee el LDR` encima de `analogRead(pines::LDR)` no.
- Rangos y unidades de campos que no se deducen del nombre.
- Sin separadores decorativos (`// -----`), sin código comentado (para eso está git), sin `TODO` sin nombre: `// TODO(kelly): ...`.

## 8. Formato

Lo define `.clang-format` (base Google, 2 espacios, 100 columnas). No se discute: se formatea con Ctrl+T y listo.

## 9. Git

- Una rama por tarea: `ranita/sensor-sombra`, `osito/buffer-littlefs`, `comun/cifrado`.
- PR hacia `main`; lo revisa alguien que **no** lo escribió. Cambios a `OrquestaComun` los revisa alguien de otro animal, porque afectan a todos.
- Commits en español, en imperativo y cortos: `Agregar detección de sombra con histéresis`.
- Antes del PR: compila sin warnings, está formateado y no hay credenciales.
- Cambiar `protocolo.h` de forma incompatible (tamaños o significado de campos) obliga a subir `protocolo::VERSION`.
