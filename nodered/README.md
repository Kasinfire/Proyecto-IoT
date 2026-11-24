# Node-RED

Une el broker, la interfaz web y Supabase. Sirve la carpeta [`web/`](../web/) y habla con ella por
WebSocket y por rutas HTTP. Las llaves van en variables de entorno, nunca en `flows.json`.

| Archivo | Contenido |
|---|---|
| `flows.json` | Flujos: MQTT ↔ WebSocket, rutas `/api/…`, guardado en Supabase (por hacer) |

Para servir la interfaz, en `settings.js` de Node-RED: `httpStatic: '<ruta del repo>/web'`.

## Contrato con la interfaz

Es lo que la interfaz ya espera (`web/js/conexion.js` y `web/js/api.js`). El modo ensayo
(`web/js/ensayo.js`) genera exactamente estos mensajes, así que sirve de ejemplo.

### WebSocket `/ws/orquesta` (Node-RED → interfaz, Publish/Subscribe)

Un mensaje JSON por evento, ya descifrado:

| `tipo` | Ejemplo | Cuándo |
|---|---|---|
| `sesion` | `{"tipo":"sesion","alias":"A-07","inicio":1795470000000,"modo":"cancion","cancion":"Estrellita","resumen":{"gestos":{"gatito":142,"ranita":96,"pollito":61},"simon":{"aciertos":9,"turnos":12},"juntos_s":250}}` | Al conectarse una pantalla, para que no empiece en cero |
| `estado` | `{"tipo":"estado","nodo":"pollito","conectado":false}` | Cambia el latido de un animal (`orquesta/{nodo}/estado`) |
| `nota` | `{"tipo":"nota","nodo":"gatito","nota":"mi","vel":90}` | Tecla del gatito; `vel: 0` = la soltó |
| `sombra` | `{"tipo":"sombra","nodo":"ranita","nivel":60}` | Sombra sobre la ranita, 0 a 100 |
| `agitar` | `{"tipo":"agitar","nodo":"pollito","intensidad":70}` | Sacudida del pollito, 0 a 100 |
| `volumen` | `{"tipo":"volumen","nodo":"osito","valor":70}` | Potenciómetro del osito |
| `modo` | `{"tipo":"modo","id":15,"ok":true,"modo":"simon","cancion":"Estrellita"}` | Respuesta del osito (`orquesta/control/modo/ack`), también si cambiaron el modo con su botón |
| `simon` | `{"tipo":"simon","nodo":"ranita"}` y luego `{"tipo":"simon","nodo":"ranita","resultado":"acierto"}` | El osito pidió un animal; luego el resultado (`acierto`, `error` o `tiempo`) |
| `cancion` | `{"tipo":"cancion","progreso":0.4}` | Avance de la canción, 0 a 1 (opcional) |
| `red` | `{"tipo":"red","internet":false,"pendientes":37}` y al volver `{"tipo":"red","internet":true,"pendientes":0,"recuperados":37,"guardado":1795470500000}` | Estado del internet y del buffer del osito |

### Rutas HTTP (interfaz → Node-RED, Request/Response)

| Ruta | Envía | Responde | Qué hace Node-RED |
|---|---|---|---|
| `POST /api/modo` | `{"modo":"cancion","cancion":"Noche de paz"}` | `{"id":15}` | Publica en `orquesta/control/modo`; la confirmación llega por el WebSocket |
| `POST /api/aviso` | `{"silenciar":true}` | `204` | Silencia o reactiva el sonido de aviso (el LED sigue) |
| `POST /api/sesion/terminar` | `{"nota":"…"}` | `204` | Cierra la sesión y la guarda en Supabase |
| `GET /api/sesion/actual` | — | `{"alias":"A-07","inicio":…}` o `null` | |
| `GET /api/canciones` | — | `[{"nombre":"Estrellita","tipo":"tranquilas","duracion":"1:20"}]` | `tipo`: `tranquilas`, `alegres`, `navidad` o `cumpleanos` |
| `GET /api/ninos` | — | `[{"alias":"A-07","sesiones":6,"simon_pct":75,"favorito":"gatito","en_sesion":true}]` | |
| `POST /api/ninos` | `{"alias":"D-04"}` | `204` | Solo alias, nunca nombres |
| `GET /api/historial?alias=A-07&periodo=mes` | — | `{"sesiones":[{"fecha":"2026-11-20","modos":["cancion","simon"],"minutos":18,"cancion":"Estrellita","gestos":299,"simon":[9,12],"aviso":"Pollito se desconectó"}],"reparto":{"gatito":742,"ranita":510,"pollito":288},"nota":{"texto":"…","fecha":"2026-11-20"}}` | Consulta Supabase; `periodo`: `mes`, `trimestre` o `todo` |
