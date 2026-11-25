# Orquesta de animalitos: Proyecto final IoT

Cuatro animalitos con ESP32 que tocan juntos una canción, pensados para musicoterapia infantil.
La especificación completa está en [`orquesta-proyecto.md`](orquesta-proyecto.md).

## Estructura

| Carpeta | Contenido |
|---|---|
| `firmware/libraries/OrquestaComun/` | Librería compartida por los 4 ESP32 (protocolo ESP-NOW, cifrado, credenciales) |
| `firmware/ranita/` | 🐸 Ranita: acompañamiento |
| `firmware/gatito/` | 🐱 Gatito: melodía |
| `firmware/pollito/` | 🐥 Pollito: percusión |
| `firmware/osito/` | 🧸 Osito: gateway y director musical |
| `broker/` | Mosquitto TLS, ACL de mínimo privilegio y password_file ignorado |
| `certs/` | CA y certificado público del broker; las llaves privadas quedan en `.secrets/` |
| `nodered/` | Flujos de Node-RED y contrato con la interfaz web |
| `web/` | Interfaz web: pantallas del terapeuta y del niño (la sirve Node-RED) |
| `herramientas/` | Scripts: aprovisionamiento, sintetizador, arranque de Node-RED |
| `docs/reporte/` | Reporte técnico |
| `docs/pruebas/` | Matriz de pruebas de estrés y evidencias |
| `docs/diseno/` | Diseño de la interfaz y auditoría de accesibilidad |

## Reglas

- **Ninguna credencial en el código** (SSID, contraseñas, claves AES). Van en `.secrets/`, que no se sube.
- El osito usa MQTTS en 8883, valida `certs/ca.crt` y arranca ESP-NOW solo después de asociarse al Wi-Fi; las reconexiones MQTT no reinician la radio.
- Preparación local: `./herramientas/preparar_mqtt.sh` crea secretos/password_file ignorados y certificados públicos; rellena `WIFI_SSID` y `WIFI_PASSWORD` solo en `.secrets/mqtt.env`. Ejecuta Mosquitto desde la raíz con `mosquitto -c broker/mosquitto.conf`. Si Mosquitto de Homebrew corre como servicio, detenlo antes (`brew services stop mosquitto`): ocupa el 8883 con otro certificado.
- Aprovisionamiento sin copiar secretos a mano: carga `firmware/aprovisionar` y corre `python3 herramientas/aprovisionar.py <puerto> osito` (o `<animal> --mac-osito … --canal …`, con el canal que imprime el osito al conectarse al Wi-Fi). No muestra ningún valor.
- Cada quien trabaja en su propia rama y la integra a `main` con un pull request.
- Todo el firmware sigue el [estándar de código](docs/estandar-codigo.md). En Arduino IDE, poner el *Sketchbook location* en `firmware/`.
