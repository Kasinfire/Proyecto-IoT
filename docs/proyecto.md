# Orquesta de animalitos: documentación del proyecto

Estado al 23/11/2026, según lo que hay en `main` del repositorio. La especificación completa está en
[`orquesta-proyecto.md`](../orquesta-proyecto.md); este documento describe lo que ya está construido.

## 1. Descripción general

Cuatro animalitos de foamy con ESP32 tocan juntos una sola canción, como herramienta de musicoterapia
infantil. Cada animal controla una parte de la canción, y esa parte solo suena mientras el niño lo está
tocando. Si tocan todos, suena completa, lo que fomenta la cooperación.

Proyecto final de Internet de las Cosas (UdeG, CUCEI, Ingeniería Informática, sección D03, 2026B),
equipo de 5. Categoría de la rúbrica: *sistema de salud conectado*.

| Animal | Parte de la canción | Sensor | Actuador |
|---|---|---|---|
| 🐱 Gatito | Melodía (piano) | 7 botones, do a si | 7 LEDs, uno por tecla |
| 🐸 Ranita | Acompañamiento (violín) | Fotorresistencia (LDR): sombra de la mano | 2 LEDs (mejillas) |
| 🐥 Pollito | Percusión (maraca) | Sensor de inclinación SW-520D: detecta que lo agitan | 1 LED |
| 🧸 Osito | Base (bajo), siempre suena; gateway y director | Potenciómetro de volumen y botón de modo | LEDs de la pancita y bocina |

### Modos

- **Canción**: la base del osito suena siempre; cada parte entra mientras su animal está activo (hasta
  2 s después de su último gesto) y sale con un fade de 150 ms.
- **Libre**: cada gesto toca una nota suelta de la escala pentatónica.
- **Simón Dice**: el osito ilumina un animal (y en el gatito, una tecla) y el niño debe tocar ese.
  Se cuentan aciertos y errores.

El botón de la pancita del osito cambia de modo: Canción → Libre → Simón.

## 2. Arquitectura

```
 Ranita ──┐
 Gatito ──┼── ESP-NOW cifrado (AES-128-GCM) ──► Osito (gateway + director musical)
 Pollito ─┘   ◄── latido, modo y luz (clave de grupo)   │
                                                        ├── USB serial ──► Laptop: sintetizador (sonido)
                                                        │
                                                        └── MQTTS 8883 ──► Mosquitto ──► Node-RED ──► Dashboard
                                                                                            │
                                                                                            └── HTTPS ──► Supabase
```

- **Percepción:** sensores y actuadores de cada animal.
- **Red:** ESP-NOW entre animales y osito; Wi-Fi solo en el osito.
- **Middleware:** Mosquitto y Node-RED en la laptop; Supabase como nube para el historial de sesiones.
- **Aplicación:** dashboard de Node-RED para el terapeuta.

Toda la lógica musical vive en el osito (edge): el sonido debe responder en menos de ~50 ms y la música
debe seguir aunque se caiga la red. La nube solo guarda historial.

> Implementado hasta ahora: la capa de animales ↔ osito por ESP-NOW. El tramo MQTT existe en el osito
> como contrato y transporte simulado; `broker/`, `nodered/`, `certs/` y `herramientas/` aún están vacíos.

## 3. Protocolo ESP-NOW

Definido en `firmware/libraries/OrquestaComun/src/protocolo.h` (versión 2).

Un mensaje es `Cabecera (en claro) || nonce (12 B) || payload cifrado || tag (16 B)`. La cabecera va como
AAD de AES-GCM: si alguien la altera, el tag deja de cuadrar y el mensaje se descarta.

| Campo de la cabecera | Tamaño | Para qué |
|---|---|---|
| `version` | 1 B | Versión del protocolo |
| `tipo` | 1 B | `TipoMensaje` |
| `nodo_emisor` | 1 B | Osito 0, Ranita 1, Gatito 2, Pollito 3 |
| `arranque` | 2 B | Contador de arranques guardado en NVS |
| `seq` | 4 B | Contador de mensajes desde el arranque |

| Tipo | Dirección | Payload |
|---|---|---|
| `Latido` (0) | todos, cada 1 s | sin payload |
| `Nota` (1) | gatito → osito | nota (do–si), velocidad (0 = soltó) |
| `Sombra` (2) | ranita → osito | nivel 0–100 |
| `Agitar` (3) | pollito → osito | intensidad 0–100 |
| `Modo` (10) | osito → animales | modo actual |
| `Luz` (11) | osito → animales | nodo destino, LED, duración (Simón Dice) |

**Claves:** cada animal tiene su clave de enlace propia hacia el osito; el osito usa una clave de grupo
para mandar el mismo mensaje a todos. Un animal se considera desconectado tras 5 s sin mensajes válidos.

**Contra replay:** el receptor guarda por emisor el último `(arranque, seq)` aceptado y rechaza cualquiera
que no sea mayor.

## 4. Firmware

Arduino IDE 2, core esp32 3.3.x, placa *ESP32 Dev Module*. El *Sketchbook location* es `firmware/`.
Todo sigue [`estandar-codigo.md`](estandar-codigo.md): cada módulo es un `namespace` con `iniciar()` y
`actualizar()`, pines en `pines.h`, ajustes en `config.h` y nada de `delay()`.

### Librería `OrquestaComun`

| Archivo | Para qué |
|---|---|
| `protocolo.h` | Formato de los mensajes ESP-NOW |
| `enlace` | Envío y recepción ESP-NOW cifrados, latido, `estaConectado()` |
| `cifrado` | AES-128-GCM con `mbedtls` |
| `contra_replay.h` | Rechaza `(arranque, seq)` repetidos o menores |
| `cola_circular.h` | Cola de tamaño fijo |
| `promedio_movil.h` | Suaviza lecturas del ADC |
| `antirrebote` | Antirrebote de botones sin `delay()` |

### Sketches

| Sketch | Qué hace | Módulos |
|---|---|---|
| `ranita` | Calibra la luz ambiente 2 s al arrancar, mide la sombra sobre la LDR (GPIO 34) y la manda como nivel 0–100; repite el nivel cada 500 ms si la mano sigue encima. Prende las mejillas cuando el osito lo pide | `sensor_sombra`, `mejillas`, `mensajes_osito` |
| `gatito` | Lee 7 teclas con antirrebote de 25 ms y prende el LED de cada tecla | `teclado`, `luces`, `enlace_simulado` |
| `pollito` | Cuenta los cambios del SW-520D en ventanas de 120 ms, calcula la intensidad y destella el LED. Prende el LED cuando el osito lo pide | `sensor_agitacion`, `led_estado`, `mensajes_osito` |
| `osito` | Recibe a los animales, lleva conexión y actividad, decide qué suena según el modo y prepara los eventos MQTT | ver abajo |
| `aprovisionar` | Guarda por Serial en NVS las claves, la MAC del osito y el canal Wi-Fi | — |
| `prueba_enlace` | Prueba del enlace con dos placas | — |
| `prueba_antirrebote` | Prueba de `Antirrebote` | — |

### Módulos del osito

| Módulo | Para qué |
|---|---|
| `animales` | Recibe por ESP-NOW y manda el latido del osito |
| `interprete` | Valida y convierte los payloads en eventos |
| `estado_animales` | Conexión (latido < 5 s), actividad (gesto < 2 s) y alertas |
| `director`, `simon`, `canciones.h` | Modos Canción, Libre y Simón Dice |
| `entradas`, `filtro_boton` | Potenciómetro (GPIO 34) y botón de modo (GPIO 27) |
| `luces` | LEDs de modo (GPIO 18 y 19) y de alerta (GPIO 21) |
| `sintetizador.h` | Interfaz de audio; hoy `SintetizadorRegistro` solo imprime las notas |
| `contrato_mqtt`, `cola_eventos` | JSON, tópicos MQTT y cola de 64 eventos que espera el acuse |
| `nube`, `transporte_*` | Puente al broker; por ahora `TransporteSimulado` |
| `simulacion` | Comandos `sim ...` por el monitor serie para probar sin las otras placas |

Canales MIDI por parte: osito 1 (bajo), ranita 2 (violín), gatito 3 (piano), pollito 10 (percusión).

### Simón Dice

1. El osito elige un animal conectado (en el gatito, también una tecla).
2. Manda `PayloadLuz` con la clave de grupo; el animal destino prende su LED 1.5 s y los demás lo ignoran.
3. Espera el gesto hasta 5 s: acierto, error o tiempo agotado. Soltar una tecla o sombra 0 no cuentan.
4. Suena un tono de acierto o de error, pausa 1.5 s y elige otro.

## 5. Seguridad

- **Sin credenciales en el código:** claves, MAC y canal se cargan en NVS con `firmware/aprovisionar`
  desde `.secrets/`, que no se sube al repo.
- **AES-128-GCM** en ESP-NOW, con la cabecera como AAD.
- **Contra replay** por `(arranque, seq)`.
- La MAC del osito se lee de los eFuse para que no salga en ceros.

Planeado (aún no en el repo): MQTTS en 8883 con CA propia, ACLs de Mosquitto, cifrado de eventos MQTT,
API key de Supabase en variable de entorno y RLS.

## 6. Pruebas

- **Osito:** `firmware/osito/pruebas/correr.sh` corre 82 pruebas de lógica en la compu, con stubs y sin
  placa; pasan las 82. No cubren ESP-NOW, cifrado ni hardware.
- **Osito en físico (22/11/2026):** firmware cargado; LEDs de modo, potenciómetro, botón, amplificador
  PAM8403 y bocina probados.
- **Enlace:** `prueba_enlace` con dos placas.
- La matriz de pruebas de estrés (`docs/pruebas/`) está pendiente.

## 7. Equipo y flujo de trabajo

Cada integrante trabaja en su propia rama y la integra a `main` con un pull request.

| PR | Rama | Qué agregó |
|---|---|---|
| #1 | `ranita` | Firmware de la ranita: sombra y mejillas |
| #2 | `comun/promedio-movil` | Plantilla `PromedioMovil` |
| #3 | `comun/protocolo-luz-nota` | Destino en `PayloadLuz` y valores del protocolo |
| #4 | `comun/enlace` | Enlace ESP-NOW cifrado, `ColaCircular`, `ContraReplay`, aprovisionamiento |
| #5 | `comun/antirrebote` | `Antirrebote` en OrquestaComun |
| #6 | `gatito` | Teclado y luces del gatito con enlace simulado |
| #7 | `osito/orquesta-demo` | Firmware del osito: modos, MQTT simulado, pruebas |
| #8 | `pollito/sensor-agitar` | Firmware del pollito |
| #9 | `comun/aprovisionar-mac` | Leer la MAC de los eFuse |
| #10 | `ranita-simon-luz` | Mejillas de la ranita en Simón Dice |
| #11 | `pollito/simon-luz` | LED del pollito en Simón Dice |

## 8. Pendientes

- **Gatito:** pasar de `enlace_simulado` al enlace real de OrquestaComun.
- **Sonido real:** `herramientas/sintetizador.py` en la laptop; hoy el osito solo imprime las notas.
- **Canción final:** `canciones.h` solo tiene una canción de prueba.
- **MQTTS real:** broker, CA, usuario y clave del osito, Wi-Fi; el osito debe iniciar ESP-NOW después de
  conectarse al router para usar su canal.
- **Cifrado de eventos MQTT:** acordar con Node-RED la clave y la AAD.
- **`seq` de MQTT:** vuelve a 1 al prender el osito; guardarlo en NVS o agregar `arranque`.
- **Cola persistente:** los 64 eventos viven en RAM; pasarlos a LittleFS.
- **Node-RED, dashboard y Supabase:** flujos, tablas (sesiones, eventos, niños con alias) y accesibilidad WCAG.
- **Osito:** probar en físico el LED rojo de alerta y decidir si migra al `Antirrebote` compartido.
- **Pruebas de estrés** y reporte técnico.
