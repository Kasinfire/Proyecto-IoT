# Node-RED

Une el broker, la interfaz web y Supabase. Sirve la carpeta [`web/`](../web/) y habla con ella por
WebSocket y por rutas HTTP. Las llaves van en variables de entorno, nunca en `flows.json`.

| Archivo | Contenido |
|---|---|
| `flows.json`, `flows_cred.json` | Flujos: MQTT ↔ WebSocket y rutas `/api/…`. Las credenciales son `${NODERED_USER}` y `${NODERED_PASSWORD}`, que salen del entorno |
| `orquesta.js` | Lo que hacen los flujos: descifra, confirma (`sync/ack`), lleva la sesión y responde a las pantallas |
| `nube.js`, `supabase.sql` | Copia de niños y sesiones en Supabase y las tablas que hay que crear allá |
| `settings.js` | Configuración: solo escucha en esta laptop, editor apagado |
| `pruebas/` | `node --test nodered/pruebas/*.test.js` |

## Arrancar

```sh
mosquitto -c broker/mosquitto.conf        # en una terminal
./herramientas/iniciar_nodered.sh         # en otra
```

Y abrir <http://127.0.0.1:1880/>. Con `ORQUESTA_EDITOR=1` se abre el editor en `/editor` (los cambios
se hacen en una copia: para conservarlos hay que pasarlos a `flows.json`).

Los datos quedan en `nodered/.datos/orquesta.json` (ignorado por git). Un evento solo se confirma al
osito después de guardarlo.

## Supabase

El archivo local es la fuente de verdad: así todo funciona sin internet. Supabase guarda una copia
de niños (solo alias y avatar) y sesiones (sin la nota del terapeuta):

1. Cada alta, cambio o borrado de un niño y cada sesión terminada se anota en `paraLaNube`, que se
   guarda en el mismo archivo antes de responder a la pantalla.
2. Cada 10 s (`subir a Supabase` en `flows.json`) se mandan en orden por la API REST. Una operación
   solo se quita cuando Supabase responde 2xx. Sin internet, con el proyecto pausado (5xx) o con la
   llave mal (401/403) se reintenta cada 10, 20, 40 y luego 60 s. Si Supabase dice que el dato no es
   válido (otro 4xx), se descarta y se avisa en la consola.
3. La primera vez que Node-RED arranca con Supabase, sube todo lo que ya había en la laptop.

El mensaje `red` lleva `nube_pendientes`. Sin `SUPABASE_URL` y `SUPABASE_KEY` (ver `COMANDOS.md`)
todo sigue igual, solo en la laptop.

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
| `panel` | `{"tipo":"panel","tempo":110,"eco":true,"ritmo":"pop","acordes":false,"loop":"sonando","capas":2,"callados":["pollito"],"nivel":3,"guia":true}` | Cambia algo del teclado DJ (`orquesta/osito/panel`) |
| `tecla` | `{"tipo":"tecla","tecla":"8"}` | Se apretó una tecla, real o en pantalla |
| `canciones` | `{"tipo":"canciones"}` | Cambió la lista: volver a pedirla |
| `grabacion` | `{"tipo":"grabacion","activa":true,"notas":5}` | Avance de la grabación del gatito |
| `cancion` | `{"tipo":"cancion","progreso":0.4}` | Avance de la canción, 0 a 1 (opcional) |
| `red` | `{"tipo":"red","internet":false,"pendientes":37}` y al volver `{"tipo":"red","internet":true,"pendientes":0,"recuperados":37,"guardado":1795470500000}` | Estado del osito y de su buffer. `broker:false` = Node-RED perdió el broker y todo pasa a `conectado:null` |

### Rutas HTTP (interfaz → Node-RED, Request/Response)

| Ruta | Envía | Responde | Qué hace Node-RED |
|---|---|---|---|
| `POST /api/modo` | `{"modo":"cancion","cancion":"Noche de paz"}` | `{"id":15}` | Publica en `orquesta/control/modo`; la confirmación llega por el WebSocket |
| `POST /api/aviso` | `{"silenciar":true}` | `204` | Silencia o reactiva el sonido de aviso (el LED sigue) |
| `POST /api/sesion/iniciar` | `{"alias":"A-07"}` | `204` | Abre la sesión con ese niño (una a la vez) |
| `POST /api/sesion/terminar` | `{"nota":"…"}` | `204` | Cierra la sesión, la guarda y la anota para subirla a Supabase |
| `GET /api/sesion/actual` | — | `{"alias":"A-07","inicio":…}` o `null` | |
| `POST /api/tecla` | `{"tecla":"8"}` | `204` | Como apretarla en el teclado del osito (`d` = D mantenida) |
| `POST /api/canciones`, `…/editar`, `…/borrar` | canción, o `{"id":…}` al borrar | `{"id":…}` o `204` | Solo las propias; las incluidas se duplican |
| `POST /api/canciones/arreglar` | canción | `{"partes":…,"pulso_ms":…}` | Las cuatro partes, para oírla antes de guardar |
| `POST /api/canciones/grabar` | `{"accion":"empezar"}`, `terminar` o `cancelar` | `204` o `{"melodia":…}` | Graba el gatito en Libre |
| `GET /api/canciones` | — | `[{"nombre":"Estrellita","tipo":"tranquilas","duracion":"1:20"}]` | `tipo`: `tranquilas`, `alegres`, `navidad` o `cumpleanos` |
| `GET /api/ninos` | — | `[{"alias":"A-07","sesiones":6,"simon_pct":75,"favorito":"gatito","avatar":null,"en_sesion":true}]` | `avatar` es `null` si no le han hecho uno |
| `POST /api/ninos` | `{"alias":"D-04","nombre":"…","avatar":{…}}` | `{"alias":"D-04"}` | `avatar` es opcional: los ids de `avatar.js`. Basta uno de los dos; sin alias se pone el siguiente libre. El nombre se queda en la laptop |
| `POST /api/ninos/editar` | `{"alias":"D-04","nuevo_alias":"D-05","nombre":"…","avatar":{…}}` | `{"alias":"D-05"}` | Sin `avatar` se queda el que tenía. Su historial se conserva |
| `POST /api/ninos/borrar` | `{"alias":"D-04"}` | `204` | Borra también sus sesiones. No si está en sesión |
| `GET /api/historial?alias=A-07&periodo=mes` | — | `{"sesiones":[{"fecha":"2026-11-20","modos":["cancion","simon"],"minutos":18,"cancion":"Estrellita","gestos":299,"simon":[9,12],"aviso":"Pollito se desconectó"}],"reparto":{"gatito":742,"ranita":510,"pollito":288}}` | Desde la copia local (funciona sin internet); `periodo`: `mes`, `trimestre` o `todo` |
