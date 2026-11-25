# Arquitectura

## Caso de uso

Orquesta de animalitos para musicoterapia infantil (categoría *sistema de salud conectado*). Cuatro
animales de foamy con ESP32 tocan juntos una canción: cada uno controla una parte y esa parte solo
suena mientras el niño lo toca. Si tocan todos, suena completa, lo que fomenta la cooperación. El
terapeuta ve la sesión en vivo y el avance de cada niño entre sesiones.

| Animal | Parte | Sensor (variante barata) | Actuador |
|---|---|---|---|
| Gatito | Melodía (piano) | 7 botones, do a si (GPIO) | 7 LEDs |
| Ranita | Acompañamiento (violín) | Fotorresistencia en divisor (ADC): sombra de la mano | 2 LEDs (mejillas) |
| Pollito | Percusión | Sensor de inclinación SW-520D (GPIO) | 1 LED |
| Osito | Base (bajo); gateway y director | Potenciómetro (ADC), botón de modo, teclado 4x4 | LEDs de modo y de alerta, bocina (sonido por la laptop) |

Modos: **Canción**, **Libre** (escala pentatónica) y **Simón Dice** (el osito ilumina un animal y el
niño debe tocar ese).

## Diagrama

```
 Ranita ──┐
 Gatito ──┼── ESP-NOW cifrado (AES-128-GCM) ──► Osito (gateway + director musical)
 Pollito ─┘   ◄── latido, modo y luz (clave de grupo)   │
                                                        ├── USB serial ──► Laptop: sintetizador.py (sonido)
                                                        │
                                                        └── MQTTS 8883 (Wi-Fi) ──► Mosquitto ──► Node-RED ──► Interfaz web
                                                                                     (laptop)      │          (terapeuta y proyector)
                                                                                                   └── HTTPS REST ──► Supabase (nube)
```

## Las cuatro capas

| Capa | Qué hay | Dónde está en el repo |
|---|---|---|
| Percepción | Sensores y actuadores de los 4 animales | `firmware/{ranita,gatito,pollito,osito}/` |
| Red | ESP-NOW cifrado entre animales y osito; Wi-Fi + MQTTS solo en el osito | `firmware/libraries/OrquestaComun/src/enlace.*`, `firmware/osito/transporte_mqtt_real.*` |
| Middleware | Mosquitto con TLS y ACL; Node-RED descifra, confirma, guarda y sube a Supabase | `broker/`, `nodered/` |
| Aplicación | Interfaz web propia: en vivo, historial, niños, canciones y pantalla del niño | `web/` |

## Edge vs remoto

| Dónde | Qué procesa | Por qué ahí |
|---|---|---|
| ESP32 de cada animal | Lectura de sensores, antirrebote, calibración de luz, detección del gesto (tecla, sombra, agitar), LED | El gesto se reduce a un evento de pocos bytes: no se manda la señal cruda |
| ESP32 del osito (edge) | Qué suena según el modo, secuencia de la canción, Simón Dice, conexión de cada animal (latido < 5 s), alerta, cola offline | El sonido debe responder en < ~50 ms y la música debe seguir aunque se caiga la red |
| Laptop: sintetizador | Convierte las notas en audio | El ESP32 no tiene un sintetizador de calidad; el componente real sería un SAM2695 |
| Laptop: Node-RED | Descifra, quita duplicados, lleva la sesión, sirve la interfaz | Varias pantallas y el historial; no afecta al sonido |
| Nube: Supabase | Copia del historial por niño (alias) | Consultar el avance desde otro equipo y respaldo; tolera estar caído |

La música nunca pasa por la red: si se cae el Wi-Fi, solo se retrasa lo que ve el terapeuta.

## Conectividad (Unidad 2)

- **ESP-NOW** entre animales y osito: no necesita router, latencia de pocos ms y menos consumo que
  mantener 4 conexiones Wi-Fi. Comparte radio con el Wi-Fi, así que todos usan el canal del router
  (se guarda en NVS al aprovisionar).
- **Wi-Fi** solo en el osito, para llegar al broker.
- **Sin BLE**: el aprovisionamiento es por cable serial (`firmware/aprovisionar`,
  `herramientas/aprovisionar.py`), así que no aplica el servicio GATT propio.

## Request/Response y Publish/Subscribe (Unidad 3)

| Patrón | Dónde | Por qué |
|---|---|---|
| Pub/Sub | Eventos de los animales (`orquesta/{animal}/evento`), estados retenidos, volumen, panel; Node-RED → pantallas por WebSocket | Flujo continuo con varios consumidores (pantalla en vivo, sesión, historial) sin que el osito sepa quién escucha |
| Request/Response sobre MQTT | Cambio de modo (`orquesta/control/modo` → `…/modo/ack` con el mismo `id`) y carga de canciones | La pantalla necesita saber que el osito sí cambió de modo |
| Acuse de entrega | `orquesta/sync/ack` con `arranque` y `hasta_seq` | El osito solo borra lo que Node-RED ya guardó (QoS 0 no lo garantiza) |
| Request/Response HTTP | Interfaz → Node-RED (`/api/...`); Node-RED → Supabase (REST, espera el código HTTP) | Acciones puntuales con respuesta: crear niño, iniciar sesión, consultar historial |
