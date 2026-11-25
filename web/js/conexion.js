// Conexión en vivo con Node-RED por WebSocket.
// Node-RED manda aquí los eventos de MQTT ya descifrados (ver nodered/README.md).
// Si se cae, reintenta solo: primero rápido y luego cada vez más espaciado.

import { iniciarSimulacion } from './ensayo.js';

const RUTA_WS = '/ws/orquesta';
const ESPERA_MIN_MS = 1000;
const ESPERA_MAX_MS = 10000;

let simulacion = null;

// La página está en ensayo si se abrió con ?ensayo o directo desde el archivo.
export function esEnsayo() {
  return new URLSearchParams(location.search).has('ensayo') || location.protocol === 'file:';
}

// Mientras se está en ensayo, el simulador hace las veces de Node-RED.
export function simulador() {
  return simulacion;
}

// alEvento(msg): cada mensaje que llega.
// alEstado(estado): 'conectando' | 'conectado' | 'sin-conexion' | 'ensayo'.
export function conectar({ alEvento, alEstado = () => {} }) {
  if (esEnsayo()) {
    alEstado('ensayo');
    simulacion = iniciarSimulacion((msg) => setTimeout(() => alEvento(msg), 0));
    return;
  }

  let espera = ESPERA_MIN_MS;
  const protocolo = location.protocol === 'https:' ? 'wss:' : 'ws:';

  const abrir = () => {
    alEstado('conectando');
    const ws = new WebSocket(`${protocolo}//${location.host}${RUTA_WS}`);

    ws.addEventListener('open', () => {
      espera = ESPERA_MIN_MS;
      alEstado('conectado');
    });

    ws.addEventListener('message', (e) => {
      let msg;
      try {
        msg = JSON.parse(e.data);
      } catch {
        console.warn('[conexion] Mensaje que no es JSON, se ignora:', e.data);
        return;
      }
      alEvento(msg);
    });

    ws.addEventListener('close', () => {
      alEstado('sin-conexion');
      setTimeout(abrir, espera);
      espera = Math.min(espera * 2, ESPERA_MAX_MS);
    });
  };

  abrir();
}
