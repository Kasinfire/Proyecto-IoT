// Peticiones HTTP a Node-RED. Las pantallas nunca hablan con el osito ni con Supabase directo:
// Node-RED publica en MQTT o consulta Supabase y responde aquí.
// En modo ensayo responde el simulador con datos de ejemplo.

import { esEnsayo, simulador } from './conexion.js';
import * as ensayo from './ensayo.js';

const ESPERA_MAX_MS = 8000;

async function pedir(ruta, opciones = {}) {
  const control = new AbortController();
  const reloj = setTimeout(() => control.abort(), ESPERA_MAX_MS);
  try {
    const resp = await fetch(ruta, {
      ...opciones,
      headers: { 'Content-Type': 'application/json' },
      signal: control.signal,
    });
    if (!resp.ok) throw new Error(`Node-RED respondió ${resp.status} en ${ruta}`);
    return resp.status === 204 ? null : await resp.json();
  } finally {
    clearTimeout(reloj);
  }
}

const enviar = (ruta, datos) => pedir(ruta, { method: 'POST', body: JSON.stringify(datos) });

// Pide al osito cambiar de modo o de canción. Devuelve { id }; la confirmación
// llega después por el WebSocket como { tipo: 'modo', id, ok, modo, cancion }.
export function pedirModo(modo, cancion) {
  if (esEnsayo()) return Promise.resolve(simulador()?.pedirModo(modo, cancion) ?? { id: 0 });
  return enviar('/api/modo', { modo, cancion });
}

// Silencia o reactiva el sonido de aviso del sintetizador (el LED del osito sigue encendido).
export function silenciarAviso(silenciar) {
  if (esEnsayo()) return Promise.resolve(null);
  return enviar('/api/aviso', { silenciar });
}

export function terminarSesion(nota) {
  if (esEnsayo()) return Promise.resolve(null);
  return enviar('/api/sesion/terminar', { nota });
}

// { alias, inicio } o null si no hay sesión abierta.
export function sesionActual() {
  if (esEnsayo()) return Promise.resolve(ensayo.sesionActual());
  return pedir('/api/sesion/actual');
}

export function obtenerCanciones() {
  if (esEnsayo()) return Promise.resolve(ensayo.CANCIONES);
  return pedir('/api/canciones');
}

export function obtenerNinos() {
  if (esEnsayo()) return Promise.resolve(ensayo.NINOS);
  return pedir('/api/ninos');
}

export function agregarNino(alias) {
  if (esEnsayo()) {
    ensayo.NINOS.push({ alias, sesiones: 0, simon_pct: null, favorito: null, en_sesion: false });
    return Promise.resolve(null);
  }
  return enviar('/api/ninos', { alias });
}

export function obtenerHistorial(alias, periodo) {
  if (esEnsayo()) return Promise.resolve(ensayo.historialDe(alias));
  return pedir(`/api/historial?alias=${encodeURIComponent(alias)}&periodo=${encodeURIComponent(periodo)}`);
}
