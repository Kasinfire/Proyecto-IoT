# Proyecto final IoT: Orquesta de animalitos para musicoterapia infantil

Este documento describe el proyecto final de la materia **Internet de las Cosas** (UdeG, CUCEI, Ingeniería Informática, sección D03, 2026B). Equipo de 5 integrantes, por lo tanto se entrega **1 solución IoT**. Contamos con **4 ESP32 (clásicas, ESP32 Dev Module)**.

Este proyecto **reutiliza la infraestructura de seguridad de la Práctica 4** (TLS con CA propia, AES-128-GCM, credenciales en NVS). Si el repo de la Práctica 4 está disponible, partir de sus archivos: `credenciales.cpp`, `cifrado_aes.cpp`, `ca_cert.h`, `certs/generar_certs.sh`, `herramientas/aprovisionar.py`, `herramientas/iniciar_nodered.sh` y el nodo function "Descifrar AES-GCM" de Node-RED.

---

## 1. Caso de uso

Herramienta para terapeutas que trabajan con niños (motricidad fina, atención, autismo, rehabilitación). Cuatro animalitos de **foamy moldeable** son instrumentos. Entre todos tocan **una sola canción**: cada animal controla una parte, y esa parte solo suena mientras el niño está tocando ese animal. Si nadie toca, solo suena la base. Si tocan todos, suena la canción completa, lo que fomenta la cooperación.

El terapeuta ve en un dashboard el estado de cada animal, las notas tocadas en vivo, estadísticas de la sesión y puede cambiar de modo. Categoría de la rúbrica: *sistema de salud conectado*.

### Modos
- **Canción** (principal): el osito reproduce la canción; cada parte se activa cuando su animal está activo. Un animal sigue "activo" ~2 s después de su último gesto. Las partes entran y salen con fade corto.
- **Libre**: cada animal toca notas sueltas en escala pentatónica (siempre suena armonioso).
- **Simón Dice**: el osito ilumina un animal / una tecla y el niño debe tocar esa. Se registran aciertos y errores.

Canciones sugeridas (dominio público / tradicionales): "Estrellita", "Los pollitos dicen".

---

## 2. Nodos y hardware

Hay dos variantes. **La base del proyecto es la lista barata** (presupuesto ~$100 MXN por integrante, ~$500 total). La lista original queda como mejora opcional y como "componentes reales" a documentar en el reporte (la rúbrica permite sustitutos si se documentan los reales).

### Asignación de partes musicales
| Nodo | Parte de la canción | Instrumento |
|---|---|---|
| 🐱 Gatito | Melodía | Piano |
| 🐸 Ranita | Acompañamiento | Violín / flauta |
| 🐥 Pollito | Percusión | Batería / maraca |
| 🧸 Osito | Base (bajo y ritmo), siempre suena | Bajo |

### 2.1 Lista barata (variante a implementar)
Alimentación de todos los nodos por **cable USB** (laptop o power bank). Sin baterías.

| Nodo | Sensor | Actuador |
|---|---|---|
| 🐸 Ranita | Fotorresistencia (LDR) + resistencia 10k en divisor (ADC). La sombra de la mano cambia la nota/volumen, como theremín | 2 LEDs normales (mejillas) |
| 🐱 Gatito | 7 push buttons (do–si) bajo teclas de foamy | 7 LEDs normales, uno por tecla, con resistencias de 220 Ω |
| 🐥 Pollito | Sensor de inclinación SW-520D (o vibración SW-420), detecta cuando lo agitan | 1 LED |
| 🧸 Osito (gateway) | Potenciómetro de volumen (ADC) + push button de cambio de modo | LEDs de la pancita (modo y ritmo) + **sonido por la laptop** (ver sección 4) |

### 2.2 Lista original (mejora opcional / componentes reales a documentar)
| Nodo | Sensor | Actuador |
|---|---|---|
| Todos | Divisor 100k/100k para leer batería por ADC | LiPo 3.7 V 1000 mAh + cargador TP4056 |
| 🐸 Ranita | VL53L0X (distancia, I2C) | 2 LEDs de tira WS2812 |
| 🐱 Gatito | 7 botones arcade 30 mm | 7 LEDs de tira WS2812 |
| 🐥 Pollito | MPU6050 (acelerómetro, I2C) | Motor vibrador 3 V con transistor 2N2222 + 1k |
| 🧸 Osito | Potenciómetro + botón | Módulo sintetizador MIDI SAM2695 (o VS1053) + bocina 8 Ω 3 W, LEDs WS2812 |

La tira WS2812 es **una sola tira** cortada en pedazos (2 ranita, 7 gatito, unos cuantos osito). Cada pedazo va a su ESP32: 5V→VIN, GND→GND, DIN→GPIO con resistencia de 330 Ω. Librería FastLED, brillo bajo (~50/255).

### 2.3 Pines propuestos (variante barata)
Reglas: con Wi-Fi o ESP-NOW activos **ADC2 no funciona**, usar solo ADC1 (GPIO 32–39). Evitar pines de arranque (0, 2, 12, 15) para entradas. GPIO 34–39 son solo entrada y no tienen pull-up interno.

**Ranita**
- LDR (divisor con 10k): GPIO 34 (ADC1)
- LED mejilla izq: GPIO 25 (220 Ω)
- LED mejilla der: GPIO 26 (220 Ω)

**Gatito**
- Botones (INPUT_PULLUP, a GND): do 13, re 14, mi 27, fa 26, sol 25, la 33, si 32
- LEDs (220 Ω): do 16, re 17, mi 18, fa 19, sol 21, la 22, si 23

**Pollito**
- Sensor de inclinación (INPUT_PULLUP, a GND): GPIO 27
- LED: GPIO 4
- (Opcional MPU6050: SDA 21, SCL 22. Motor: base del 2N2222 por 1k en GPIO 25)

**Osito**
- Potenciómetro de volumen: GPIO 34 (ADC1)
- Botón de modo (INPUT_PULLUP): GPIO 27
- LEDs pancita (220 Ω): GPIO 18, 19, 21
- (Opcional SAM2695: TX de UART2 en GPIO 17 a 31250 baudios)

Validar contra el circuito real antes de soldar.

---

## 3. Arquitectura (4 capas)

```
Ranita / Gatito / Pollito --ESP-NOW cifrado (AES-GCM)--> Osito (gateway + director musical)
                                                            |  USB serial --> Laptop: sintetizador (sonido)
                                                            |  MQTTS 8883 (Wi-Fi)
                                                         Mosquitto (laptop)
                                                            |
                                                         Node-RED --> Interfaz web (terapeuta y proyector)
                                                            |  HTTPS (REST)
                                                         Supabase (nube: historial de sesiones)
```

- **Percepción:** sensores y actuadores de cada animal.
- **Red:** ESP-NOW entre animales y osito; Wi-Fi solo en el osito.
- **Middleware:** Mosquitto (en la laptop, configuración de la Práctica 4) + Node-RED. Nube: **Supabase** (Postgres + API REST), donde Node-RED guarda el historial de sesiones.
- **Aplicación:** interfaz web propia (`web/`: HTML, CSS y JavaScript sin dependencias) que sirve Node-RED. Recibe los eventos por WebSocket y pide acciones e historial por HTTP a Node-RED; nunca habla directo con el osito ni con Supabase. Se eligió en lugar de Dashboard 2.0 para poder aplicar el diseño centrado en el usuario de [`docs/diseno/`](docs/diseno/).

### Edge vs remoto
- **En el ESP32 (edge):** lectura de sensores, antirrebote, detección de gesto (agitar, sombra, tecla), decisión de qué nota/parte toca, secuencia de la canción, lógica de modos y Simón Dice, LEDs, buffer offline. Justificación: el sonido debe responder en < ~50 ms; si dependiera de la red se sentiría desfasado, y la música debe seguir aunque se caiga el internet.
- **Remoto (Node-RED en la laptop):** partitura en vivo, alertas, cambio de modo desde el dashboard.
- **Remoto (Supabase, nube):** historial de sesiones y estadísticas por niño, para ver el avance entre sesiones.
- La música nunca pasa por la nube, así que la latencia de internet no afecta el sonido; solo retrasa un poco lo que se ve en el dashboard o en el historial.

---

## 4. Sonido

**El osito es el que decide qué suena** (lógica musical en el edge). La canción vive en el firmware del osito como arreglos por parte (nota, duración). El osito manda eventos de nota a un sintetizador:

- **Variante barata (implementar):** el osito envía por **USB serial** líneas de texto tipo `ON <canal> <nota_midi> <vel>` / `OFF <canal> <nota_midi>` / `VOL <0-127>`. Un script en la laptop (`herramientas/sintetizador.py`) las lee y las toca con instrumentos reales (por ejemplo `pyfluidsynth` + un soundfont General MIDI, o `mido` + `python-rtmidi` hacia un sintetizador del sistema). En el reporte se documenta que el componente real sería el SAM2695.
- **Variante original:** el osito manda MIDI binario por UART2 al **SAM2695** a 31250 baudios.

Diseñar el código del osito con una interfaz `Sintetizador` con dos implementaciones (serial a laptop / UART MIDI) para cambiar entre variantes sin tocar la lógica.

Notas: escala pentatónica en modo libre; canal MIDI por parte (gatito piano, ranita violín, pollito percusión canal 10, osito bajo). **No usar BLE-MIDI** (ver sección 5).

---

## 5. Conectividad (Unidad 2)

- **ESP-NOW** entre animales y osito: baja latencia, sin router de por medio.
- **Wi-Fi** solo en el osito, para llegar al broker.
- **Importante:** ESP-NOW y Wi-Fi comparten radio. Los animales deben usar **el mismo canal Wi-Fi** que el router/hotspot al que se conecta el osito. Resolverlo leyendo el canal en el osito y configurando ese canal en los animales (guardado en NVS durante el aprovisionamiento, o escaneando el SSID al arrancar).
- **Sin BLE.** El aprovisionamiento es por serial (como en la Práctica 4). Así no aplica el requisito de servicio GATT propio. Si en algún momento se agrega BLE, tendría que ser con un servicio GATT propio con UUIDs documentados.

---

## 6. MQTT y patrones (Unidad 3)

Cliente: PubSubClient sobre WiFiClientSecure (igual que Práctica 4). PubSubClient **solo publica con QoS 0**, por eso la entrega confiable se resuelve con ack a nivel aplicación (sección 8).

### Tópicos
| Tópico | Dirección | Contenido | Patrón |
|---|---|---|---|
| `orquesta/{nodo}/evento` | osito → broker | Nota/gesto, cifrado AES-GCM (Base64) | Pub/Sub |
| `orquesta/{nodo}/estado` | osito → broker | online/offline, RSSI. Retained. LWT `OFFLINE` para el osito; el osito publica el estado de cada animal según su heartbeat ESP-NOW | Pub/Sub |
| `orquesta/control/modo` | Node-RED → osito | `{"id":<n>,"modo":"cancion|libre|simon","cancion":"estrellita"}` | Request |
| `orquesta/control/modo/ack` | osito → Node-RED | `{"id":<n>,"ok":true,"modo":"cancion"}` | Response |
| `orquesta/sync/ack` | Node-RED → osito | `{"hasta_seq":<n>}` confirma eventos recibidos para borrarlos del buffer | Response |
| `orquesta/sesion/resumen` | Node-RED → Node-RED (flujo que lo guarda en Supabase) | Resumen de sesión | Pub/Sub |

Payload de evento (texto plano antes de cifrar):
```json
{"seq": 1532, "t": 1762900000, "nodo": "gatito", "tipo": "nota", "nota": "mi", "vel": 90}
```
`{nodo}` ∈ `ranita`, `gatito`, `pollito`, `osito`.

### Request/Response (justificación para el reporte)
- **Cambio de modo con confirmación:** el dashboard necesita saber que el osito realmente cambió de modo; un publish sin respuesta no lo garantiza.
- **Ack de sincronización:** el osito solo borra eventos de su buffer cuando Node-RED confirma que los recibió.
- **Historial de sesión:** el dashboard consulta `GET /api/sesion/:id` (endpoint HTTP en Node-RED), que a su vez consulta Supabase.
- **Supabase (HTTPS/REST):** Node-RED guarda con `POST` y consulta con `GET`; cada petición espera su respuesta (código HTTP) para saber si se guardó.

### Pub/Sub (justificación)
Eventos continuos y estados con varios consumidores (dashboard, registro de sesión, envío a Supabase) sin que el osito sepa quién escucha.

---

## 7. Seguridad (Unidad 4)

Reutilizar Práctica 4:
- **MQTTS** solo en 8883, CA propia, `setCACert()`, NTP antes de conectar, nunca `setInsecure()`. El SAN del certificado del broker debe incluir la IP actual de la laptop.
- **AES-128-GCM** con `mbedtls_gcm`, nonce aleatorio de 12 bytes (`esp_fill_random`), formato `nonce || cifrado || tag` en Base64. Node-RED descifra con la clave desde la variable de entorno `AES_KEY_HEX`.
- **Credenciales en NVS** con Preferences y aprovisionamiento único por serial (`aprovisionar.py` leyendo `.secrets/credenciales.env`, que no se sube al repo). **Ninguna credencial en el código**: ni SSID, ni contraseñas, ni claves AES.

Nuevo en este proyecto:
- **AES-GCM también en ESP-NOW** (animal → osito), con una clave de enlace aprovisionada en la NVS de cada animal.
- **Protección contra replay:** incluir `seq` (contador monotónico) e id de nodo como **AAD** en AES-GCM; el receptor rechaza `seq` repetidos o menores al último aceptado. Aplica en ESP-NOW y en MQTT.
- **ACLs en Mosquitto:** el usuario del osito solo publica en `orquesta/+/evento`, `orquesta/+/estado`, `orquesta/control/modo/ack`; el usuario de Node-RED solo publica en `orquesta/control/#` y `orquesta/sync/#`.
- Opcional: `adminAuth` en el editor de Node-RED.
- **Supabase:** la API key va en una variable de entorno de Node-RED (como `AES_KEY_HEX`), nunca en `flows.json`. RLS activado en las tablas. Los niños se guardan con alias o ID, nunca con su nombre real.

### OWASP IoT Top 10, abiertas (actualizar al final)
- I10: sin flash encryption ni secure boot; la NVS se puede leer con la placa en la mano.
- I4: sin OTA firmado.
- I6: datos de sesiones de niños guardados en la nube (Supabase); se usan alias, pero no hay anonimización completa.
- I3: dashboard por HTTP (y editor sin contraseña si no se agrega `adminAuth`).
- I8: si se comparte clave/usuario entre dispositivos, no se puede revocar uno solo.

---

## 8. Resiliencia (Unidad 5)

- Si se cae el Wi-Fi o el broker, **la música sigue sonando** (todo es local en el osito y la laptop).
- El osito guarda los eventos en **LittleFS** (archivo de líneas JSON con `seq` y timestamp NTP) mientras no hay conexión.
- Al reconectar, reenvía en orden. Node-RED responde `orquesta/sync/ack` con el último `seq` recibido y el osito borra hasta ahí. Node-RED descarta duplicados por `seq`.
- Limitar el tamaño del buffer (por ejemplo, descartar lo más viejo si se llena) y documentar ese límite.
- Segundo nivel (laptop → Supabase): si se cae el internet de la laptop, Node-RED guarda lo pendiente (por ejemplo, en un archivo) y reintenta cada cierto tiempo hasta que Supabase confirme. Sin esto, se perderían los datos de ese tramo.
- Nota: los proyectos gratuitos de Supabase se pausan tras varios días sin uso; abrirlo antes de la demo.

Evidencia: apagar el router/hotspot a mitad de una sesión y mostrar que el dashboard recupera todos los eventos al volver.

---

## 9. Dashboard y accesibilidad (Unidad 6)

Interfaz web servida por Node-RED (`http://<ip-laptop>:1880/`), con una pantalla para el terapeuta y otra para el niño en el proyector. La pantalla del terapeuta muestra:
- Estado de cada animal (conectado, activo ahora).
- Partitura en vivo: notas tocadas con el color de cada animal.
- Control: modo (canción / libre / Simón Dice), selección de canción, con indicador de confirmación (ack).
- Sesión: notas por animal, aciertos de Simón Dice, duración.
- Alertas.

WCAG a auditar:
- **1.4.3** contraste de texto ≥ 4.5:1 (≥ 3:1 texto grande).
- **1.4.11** contraste de componentes y gráficos ≥ 3:1.
- **2.5.8** objetivos táctiles de al menos 24×24 px.
- No transmitir información solo por color: cada color va acompañado del nombre del animal o un ícono.

**Estado crítico multicanal:** *animalito desconectado* (sin heartbeat ESP-NOW por X segundos) → LED rojo en el osito + sonido de aviso por el sintetizador + alerta en el dashboard.

---

## 10. Costo y energía

- Variante barata: ~$200–230 MXN en componentes (ESP32, protoboards, cables y potenciómetro ya los teníamos).
- Variante original: ~$1,700–2,000 MXN.
- Autonomía: se calcula en el reporte con una LiPo teórica de 1000 mAh. Nodo con radio activa ~80–120 mA → ~8–10 h. Discutir light sleep entre gestos y viabilidad en un consultorio con recursos limitados.

---

## 11. Pruebas de estrés (matriz)

- Los 3 instrumentos tocando a la vez durante 5 min (pérdida de eventos, latencia).
- Router/hotspot apagado 10 min a mitad de sesión (buffer y recuperación).
- Broker detenido y reiniciado.
- Osito reiniciado a mitad de sesión.
- Animal apagado (debe dispararse la alerta multicanal).
- Distancia máxima entre animales y osito.
- Mensaje ESP-NOW o MQTT alterado o repetido (debe rechazarse por tag o por `seq`).

---

## 12. Estructura sugerida del repo

```
/firmware
  /comun          credenciales, cifrado_aes, protocolo ESP-NOW (structs, seq, AAD)
  /ranita
  /gatito
  /pollito
  /osito          gateway, secuenciador de canciones, modos, buffer LittleFS, Sintetizador
/nodered          flows.json (sin claves; usar variables de entorno) y el contrato con la interfaz
/web              interfaz: en-vivo, historial, canciones, ninos, proyector (+ css/, js/, fuentes/)
/docs/diseno      wireframe, notas de diseño y auditoría WCAG
/broker           mosquitto.conf, acl (sin contraseñas)
/certs            generar_certs.sh (sin claves privadas en el repo)
/herramientas     aprovisionar.py, iniciar_nodered.sh, sintetizador.py
.gitignore        .secrets/, *.key, passwd, credenciales.env
```

Código estructurado y comentado, **sin ninguna credencial visible** (requisito de entrega).

---

## 13. Decisiones pendientes

- Confirmar variante final de hardware por nodo (barata u original, o mezcla: por ejemplo MPU6050 en el pollito si sobra presupuesto).
- Canción(es) a incluir.
- ~~Nube~~ **Decidido:** Supabase como nube (no HiveMQ ni Google Cloud). Pendiente: diseñar las tablas (sesiones, eventos, niños con alias).
