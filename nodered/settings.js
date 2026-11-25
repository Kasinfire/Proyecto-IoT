// Configuración de Node-RED para la orquesta. Se arranca con herramientas/iniciar_nodered.sh, que
// pone los secretos en variables de entorno; aquí y en flows.json no hay ninguno.
const path = require('node:path');
const { Orquesta } = require('./orquesta.js');
const { NubeSupabase } = require('./nube.js');

const RAIZ = path.resolve(__dirname, '..');
const HOST = process.env.ORQUESTA_HOST || '127.0.0.1';
const PUERTO = Number(process.env.ORQUESTA_PUERTO || 1880);

// Solo esta laptop. El navegador no deja que otra página lea la API, pero:
// - un WebSocket sí se puede abrir desde cualquier sitio, así que se revisa de dónde viene;
// - con "DNS rebinding" una página ajena podría hablarle a 127.0.0.1 con su propio nombre: se
//   revisa el Host. Si alguien abre la interfaz a la red (ORQUESTA_HOST), no se revisa el Host.
const SOLO_LOCAL = ['127.0.0.1', 'localhost'].includes(HOST);
const NOMBRES_LOCALES = new Set([`127.0.0.1:${PUERTO}`, `localhost:${PUERTO}`]);
const hostLocal = (host) => !SOLO_LOCAL || NOMBRES_LOCALES.has(String(host || '').toLowerCase());
const origenLocal = (origen) => {
  try {
    return hostLocal(new URL(origen).host);
  } catch {
    return false;
  }
};

module.exports = {
  flowFile: 'flows.json',
  credentialSecret: false, // flows_cred.json solo trae ${NODERED_USER} y ${NODERED_PASSWORD}
  // Solo esta laptop: la interfaz no pide contraseña y maneja datos de las sesiones.
  uiHost: HOST,
  uiPort: PUERTO,
  httpNodeMiddleware: (req, res, next) => {
    if (hostLocal(req.headers.host)) return next();
    res.status(421).json({ error: 'solo desde esta laptop' });
  },
  webSocketNodeVerifyClient: (info) => hostLocal(info.req.headers.host) && origenLocal(info.origin),
  httpAdminRoot: '/editor',
  httpNodeRoot: '/',
  httpStatic: path.join(RAIZ, 'web'),
  disableEditor: process.env.ORQUESTA_EDITOR !== '1',
  // Al arrancar no se instala ni se busca nada en internet.
  externalModules: { autoInstall: false, palette: { allowInstall: false }, modules: { allowInstall: false } },
  telemetry: { enabled: false, updateNotification: false },
  functionGlobalContext: {
    orquesta: new Orquesta({
      claveHex: process.env.MQTT_AES_KEY_HEX,
      archivo: process.env.ORQUESTA_DATOS || path.join(RAIZ, 'nodered', '.datos', 'orquesta.json'),
      aviso: (texto) => console.warn(`[orquesta] ${texto}`),
      // Sin SUPABASE_URL y SUPABASE_KEY todo se queda en la laptop.
      nube: process.env.SUPABASE_URL && process.env.SUPABASE_KEY
        ? new NubeSupabase({ url: process.env.SUPABASE_URL, llave: process.env.SUPABASE_KEY })
        : null,
    }),
  },
  logging: { console: { level: 'info', metrics: false, audit: false } },
};
