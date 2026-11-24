# Interfaz web

Las pantallas del terapeuta y la del niño. Son HTML, CSS y JavaScript sencillos, sin instalar nada:
Node-RED las sirve y les manda los datos. El diseño y su justificación están en [`docs/diseno/`](../docs/diseno/).

| Archivo | Pantalla |
|---|---|
| `en-vivo.html` | Sesión en vivo: animalitos con su partitura, juego (modo y canción), conexión y resumen |
| `historial.html` | Avance de cada niño entre sesiones (viene de Supabase a través de Node-RED) |
| `canciones.html` | Biblioteca de canciones con filtros (Tranquilas, Alegres, Navidad, Cumpleaños) |
| `ninos.html` | Niños del consultorio, solo con alias |
| `proyector.html` | Pantalla del niño, estilo bloques, para el proyector |
| `index.html` | Manda a `en-vivo.html` |

| Carpeta | Contenido |
|---|---|
| `css/` | `base.css` (colores, tipografía, espacios), `componentes.css` (botones, encabezado, avisos, ventanas), `pantallas.css` (acomodo de cada pantalla) y `proyector.css` |
| `js/` | `conexion.js` (WebSocket con Node-RED, se reconecta solo), `api.js` (rutas `/api/…`), `caritas.js` (caritas pixeladas y datos de cada animal), `comun.js` (ayudas y encabezado), un archivo por pantalla y `ensayo.js` (simulador) |
| `fuentes/` | Plus Jakarta Sans y Press Start 2P con su licencia (OFL), guardadas aquí porque el consultorio puede no tener internet |

## Probarlo sin Node-RED (modo ensayo)

Cualquier pantalla abierta con `?ensayo` usa datos simulados y lo avisa arriba. Desde esta carpeta:

```sh
python3 -m http.server 8000
```

y abrir `http://localhost:8000/en-vivo.html?ensayo`. El pollito "se desconecta" a los 6 s para ver la
alerta, y en Conexión aparece **Simular caída** para ensayar la prueba de resiliencia. Ese botón solo
existe en ensayo: en la demo real se apaga el router.

Los módulos de JavaScript no cargan si se abre el archivo con doble clic (`file://`); hace falta un servidor.

## Con Node-RED

Node-RED sirve esta carpeta (en `settings.js`, `httpStatic` apuntando a `web/`) y la interfaz queda en
`http://<ip-laptop>:1880/`. Lo que manda y recibe cada pantalla está en
[`nodered/README.md`](../nodered/README.md).

## Reglas

- Las pantallas nunca hablan con el osito ni con Supabase directo: todo pasa por Node-RED.
- Ninguna llave ni contraseña aquí; el navegador no debe ver credenciales.
- Colores solo en `css/base.css`, para cambiarlos en un solo lugar.
- Una sola acción principal (violeta) por pantalla; lo elegido va en lima.
- Ningún estado solo por color: siempre ícono + palabra. Todo lo que se toca mide al menos 44 px.
