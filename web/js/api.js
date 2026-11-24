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
    if (!resp.ok) {
      const error = new Error(`Node-RED respondió ${resp.status} en ${ruta}`);
      // La causa que da Node-RED, para poder mostrarla (si no es nuestra respuesta, no hay).
      error.causa = (await resp.json().catch(() => null))?.error ?? null;
      throw error;
    }
    return resp.status === 204 ? null : await resp.json();
  } finally {
    clearTimeout(reloj);
  }
}

// Texto para la pantalla: la causa si se sabe, o que Node-RED no responde.
export function explicar(error, queFallo) {
  return error.causa ? `${queFallo}: ${error.causa}.` : `${queFallo}. Revisa que Node-RED esté encendido.`;
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

export function iniciarSesion(alias) {
  if (esEnsayo()) return Promise.resolve(null);
  return enviar('/api/sesion/iniciar', { alias });
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

export function guardarCancion(cancion) {
  if (esEnsayo()) {
    const ficha = { ...cancion, id: cancion.id ?? `m-${Date.now()}`, duracion: '0:10', incluida: false };
    const i = ensayo.CANCIONES.findIndex((c) => c.id && c.id === cancion.id);
    if (i >= 0) ensayo.CANCIONES[i] = ficha;
    else ensayo.CANCIONES.push(ficha);
    return Promise.resolve({ id: ficha.id });
  }
  return enviar(cancion.id ? '/api/canciones/editar' : '/api/canciones', cancion);
}

export function borrarCancion(id) {
  if (esEnsayo()) {
    ensayo.CANCIONES.splice(ensayo.CANCIONES.findIndex((c) => c.id === id), 1);
    return Promise.resolve(null);
  }
  return enviar('/api/canciones/borrar', { id });
}

// Las cuatro partes de la canción, para oírla. En ensayo solo la melodía.
export function arreglarCancion(cancion) {
  if (esEnsayo()) return Promise.resolve({ partes: { gatito: cancion.melodia }, pulso_ms: cancion.pulso_ms });
  return enviar('/api/canciones/arreglar', cancion);
}

// accion: 'empezar' | 'terminar' (devuelve la melodía) | 'cancelar'
export function grabarGatito(accion) {
  if (esEnsayo()) return Promise.resolve(accion === 'terminar' ? { melodia: [[60, 2], [62, 2], [64, 4]], pulso_ms: 250 } : null);
  return enviar('/api/canciones/grabar', { accion });
}

export function tocarTecla(tecla) {
  if (esEnsayo()) {
    simulador()?.tocarTecla(tecla);
    return Promise.resolve(null);
  }
  return enviar('/api/tecla', { tecla });
}

export function obtenerNinos() {
  if (esEnsayo()) return Promise.resolve(ensayo.NINOS);
  return pedir('/api/ninos');
}

// Alias vacío: Node-RED pone el siguiente libre. El nombre es opcional y se queda en la laptop.
export function agregarNino(alias, nombre) {
  if (esEnsayo()) {
    ensayo.NINOS.push({ alias: alias || `N-0${ensayo.NINOS.length + 1}`, nombre, sesiones: 0, simon_pct: null, favorito: null, en_sesion: false });
    return Promise.resolve(null);
  }
  return enviar('/api/ninos', { alias, nombre });
}

export function editarNino(alias, nuevoAlias, nombre) {
  if (esEnsayo()) {
    Object.assign(ensayo.NINOS.find((n) => n.alias === alias), { alias: nuevoAlias, nombre });
    return Promise.resolve(null);
  }
  return enviar('/api/ninos/editar', { alias, nuevo_alias: nuevoAlias, nombre });
}

export function borrarNino(alias) {
  if (esEnsayo()) {
    ensayo.NINOS.splice(ensayo.NINOS.findIndex((n) => n.alias === alias), 1);
    return Promise.resolve(null);
  }
  return enviar('/api/ninos/borrar', { alias });
}

export function obtenerHistorial(alias, periodo) {
  if (esEnsayo()) return Promise.resolve(ensayo.historialDe(alias));
  return pedir(`/api/historial?alias=${encodeURIComponent(alias)}&periodo=${encodeURIComponent(periodo)}`);
}
