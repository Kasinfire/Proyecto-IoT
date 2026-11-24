# Interfaz web

Las pantallas del terapeuta y la del niño. Son HTML, CSS y JavaScript sencillos, sin instalar nada:
Node-RED las sirve y les manda los datos. El diseño está en [`docs/diseno/`](../docs/diseno/).

| Archivo | Pantalla |
|---|---|
| `en-vivo.html` | Sesión en vivo del terapeuta (animalitos, juego, conexión, resumen) |
| `historial.html` | Avance de cada niño entre sesiones (viene de Supabase) |
| `canciones.html` | Biblioteca de canciones con filtros |
| `ninos.html` | Niños del consultorio, solo con alias |
| `proyector.html` | Pantalla del niño, estilo bloques, para el proyector |

| Carpeta | Contenido |
|---|---|
| `css/` | `base.css` (colores, tipografía, espacios) y `componentes.css` (botones, tarjetas, menú) |
| `js/` | `conexion.js` (WebSocket con Node-RED), `api.js` (rutas `/api/…`), `caritas.js` (caritas pixeladas), un archivo por pantalla y `ensayo.js` (datos simulados para probar sin Node-RED) |
| `fuentes/` | Plus Jakarta Sans y Press Start 2P, guardadas aquí porque el consultorio puede no tener internet |

Reglas:

- Las pantallas nunca hablan con el osito ni con Supabase directo: todo pasa por Node-RED.
- Ninguna llave ni contraseña aquí; el navegador no debe ver credenciales.
- Colores y botones solo en `css/`, para cambiarlos en un solo lugar.
