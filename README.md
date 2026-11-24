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
| `broker/` | Configuración de Mosquitto y ACL (sin contraseñas) |
| `certs/` | Script para generar certificados (sin llaves privadas) |
| `nodered/` | Flujos de Node-RED y contrato con la interfaz web |
| `web/` | Interfaz web: pantallas del terapeuta y del niño (la sirve Node-RED) |
| `herramientas/` | Scripts: aprovisionamiento, sintetizador, arranque de Node-RED |
| `docs/reporte/` | Reporte técnico |
| `docs/pruebas/` | Matriz de pruebas de estrés y evidencias |
| `docs/diseno/` | Diseño de la interfaz y auditoría de accesibilidad |

## Reglas

- **Ninguna credencial en el código** (SSID, contraseñas, claves AES). Van en `.secrets/`, que no se sube.
- Cada quien trabaja en su propia rama y la integra a `main` con un pull request.
- Todo el firmware sigue el [estándar de código](docs/estandar-codigo.md). En Arduino IDE, poner el *Sketchbook location* en `firmware/`.
