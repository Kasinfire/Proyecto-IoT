# Node-RED

Une el broker, la interfaz web y Supabase. Sirve la carpeta [`web/`](../web/) y habla con ella por
WebSocket y por rutas HTTP. Las llaves van en variables de entorno, nunca en `flows.json`.

| Archivo | Contenido |
|---|---|
| `flows.json` | Flujos: MQTT ↔ WebSocket, rutas `/api/…`, guardado en Supabase |

## Contrato con la interfaz

Pendiente de definir entre el equipo. Lo que ya marca la especificación:

- WebSocket: los mismos eventos de MQTT ya descifrados, por ejemplo `{"nodo":"ranita","tipo":"sombra","nivel":60}`.
- `POST /api/modo`: pide el cambio de modo; la confirmación (`orquesta/control/modo/ack`) llega por el WebSocket.
- `GET /api/sesion/:id`: historial de una sesión, consultado en Supabase.
