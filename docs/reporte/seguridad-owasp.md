# Seguridad

## Diagrama de seguridad

```
 Animal ──[ESP-NOW: AES-128-GCM, clave de enlace por animal, AAD = cabecera, anti-replay (arranque, seq)]──► Osito
 Osito  ──[ESP-NOW: AES-128-GCM, clave de grupo]──► Animales (modo, luz de Simón)
 Osito  ──[MQTTS 8883: TLS con CA propia + usuario/contraseña + ACL]──► Mosquitto
          └─ contenido: AES-128-GCM aparte (MQTT_AES_KEY_HEX), AAD = "nodo:seq", arranque dentro del cifrado
 Mosquitto ──[MQTTS, usuario nodered + ACL]──► Node-RED (solo escucha en 127.0.0.1)
 Node-RED ──[HTTPS, llave secreta en variable de entorno, RLS]──► Supabase (solo alias, sin nombre ni nota)

 Secretos: .secrets/ (fuera de git) ──aprovisionamiento por serial──► NVS de cada ESP32
                                    └─ variables de entorno ──► Node-RED
```

Se usan **las dos protecciones** que recomienda la rúbrica: canal cifrado (TLS) y contenido cifrado
aparte (AES-GCM), de modo que ni el broker ve los eventos en claro.

## Sin credenciales en el código

- Claves AES, MAC del osito, canal, SSID, contraseña Wi-Fi y del broker: en la NVS, cargadas por
  serial desde `.secrets/` (`firmware/aprovisionar`, `herramientas/aprovisionar.py`).
- Node-RED: `flows_cred.json` solo tiene `${NODERED_USER}` y `${NODERED_PASSWORD}`;
  `herramientas/iniciar_nodered.sh` los lee de `.secrets/mqtt.env`.
- `.gitignore` excluye `.secrets/`, `*.env`, `broker/passwd`, `certs/*.key`. En git solo hay
  certificados públicos (`certs/ca.crt`, `certs/broker.crt`).

## OWASP IoT Top 10 (2018)

| # | Vulnerabilidad | Estado | Qué hay / qué falta |
|---|---|---|---|
| I1 | Contraseñas débiles, adivinables o fijas | Mitigada | No hay contraseñas por defecto: `herramientas/preparar_mqtt.sh` genera las del broker al azar; el broker no acepta anónimos |
| I2 | Servicios de red inseguros | Mitigada | Un solo listener MQTTS 8883, sin 1883; Node-RED solo en `127.0.0.1` y con el editor apagado (`nodered/settings.js`) |
| I3 | Interfaces del ecosistema inseguras | Parcial | La interfaz web va por HTTP sin contraseña: solo se puede abrir desde la laptop. Si se abre a la red (`ORQUESTA_HOST`), queda expuesta |
| I4 | Sin mecanismo de actualización seguro | **Abierta** | No hay OTA; se actualiza por cable. Tampoco hay firma de firmware |
| I5 | Componentes inseguros u obsoletos | Parcial | Core esp32 3.3.x y Mosquitto 2.x actuales; no hay revisión automática de dependencias |
| I6 | Protección de la privacidad insuficiente | Parcial | A la nube solo va el alias del niño, nunca el nombre ni la nota del terapeuta (`paraNube`); RLS sin políticas públicas. Pero las sesiones siguen siendo datos de salud de menores y no hay anonimización completa ni consentimiento digital |
| I7 | Transferencia y almacenamiento inseguros | Parcial | En tránsito: AES-GCM en ESP-NOW y MQTT, TLS en MQTT y HTTPS a Supabase. En reposo: el archivo de Node-RED (`nodered/.datos/`, permisos 600) y la cola en LittleFS **no están cifrados** |
| I8 | Falta de gestión de dispositivos | Parcial | Cada animal tiene su clave de enlace (se puede cambiar una sola), pero la clave de grupo y la clave MQTT son compartidas: revocar un dispositivo obliga a re-aprovisionar todos |
| I9 | Configuración por defecto insegura | Mitigada | Sin `setInsecure()`, se valida la CA (`setCACert`); el broker rechaza anónimos; la ACL limita los tópicos de cada usuario (`broker/acl`) |
| I10 | Falta de endurecimiento físico | **Abierta** | Sin flash encryption ni secure boot: con la placa en la mano se pueden leer las claves de la NVS. El puerto USB queda accesible dentro del peluche |

### Ataques probados (pendiente de correr en físico)

- Mensaje ESP-NOW alterado → se rechaza por el tag GCM.
- Mensaje repetido → se rechaza por `(arranque, seq)` (`contra_replay.h`).
- Evento MQTT alterado o con otra clave → Node-RED lo rechaza sin confirmar
  (`nodered/pruebas/orquesta.test.js`: "rechaza alterado, clave distinta y tópico cambiado").
