// Historial de un niño: sus sesiones, su avance en Simón Dice y con qué animal toca más.
// Los datos los guarda Node-RED en Supabase y aquí se piden a Node-RED (nunca directo a Supabase).

import { cuadroCara, datosAnimal } from './caritas.js';
import { cuadroAvatar } from './avatar.js';
import { el, fechaCorta, iniciarPagina } from './comun.js';
import { explicar, obtenerHistorial, obtenerNinos } from './api.js';

const NOMBRE_MODO = { cancion: 'Canción', libre: 'Libre', simon: 'Simón Dice' };

const $ = (id) => document.getElementById(id);

let ninos = [];

function pintarSesiones(sesiones, alias) {
  const nino = ninos.find((n) => n.alias === alias);
  $('avatar').replaceChildren(...(nino ? [cuadroAvatar(nino.avatar, nino.alias, 56)] : []));
  $('t-sesiones').replaceChildren(`Sesiones de ${alias} `, el('span', { style: { fontWeight: 600 } }, `· ${sesiones.length}`));
  if (!sesiones.length) {
    $('lista-sesiones').replaceChildren(el('p', { class: 'vacio' }, 'Todavía no hay sesiones guardadas en este periodo.'));
    return;
  }
  $('lista-sesiones').replaceChildren(...sesiones.map((s) => {
    const { dia, mes } = fechaCorta(s.fecha);
    return el('article', { class: 'sesion' },
      el('div', { class: 'sesion__fecha' }, el('span', { class: 'sesion__dia' }, dia), el('span', { class: 'sesion__mes' }, mes)),
      el('div', { class: 'sesion__que' },
        el('p', { class: 'sesion__modos' }, s.modos.map((m) => NOMBRE_MODO[m]).join(' · ')),
        el('p', { class: 'texto-ayuda' }, `${s.minutos} min · ${s.cancion}`)),
      el('div', { class: 'sesion__dato' }, el('small', {}, 'Gestos'), el('strong', {}, String(s.gestos))),
      el('div', { class: 'sesion__dato' }, el('small', {}, 'Simón Dice'), el('strong', {}, s.simon ? `${s.simon[0]} de ${s.simon[1]}` : '—')),
      s.nota ? el('p', { class: 'sesion__nota' }, el('strong', {}, 'Nota: '), s.nota) : null,
      el('p', { class: `sesion__aviso${s.aviso ? ' sesion__aviso--hay' : ''}` }, s.aviso ?? 'Sin avisos'));
  }));
}

function pintarAvance(sesiones) {
  // Más antigua a la izquierda, solo las que jugaron Simón Dice
  const puntos = sesiones.filter((s) => s.simon).slice(0, 6).reverse()
    .map((s) => ({ fecha: fechaCorta(s.fecha), pct: Math.round((s.simon[0] / s.simon[1]) * 100) }));
  if (!puntos.length) {
    $('avance').replaceChildren(el('p', {}, 'Todavía no hay turnos de Simón Dice.'));
    return;
  }
  const primero = puntos[0].pct;
  const ultimo = puntos[puntos.length - 1].pct;
  $('avance').replaceChildren(
    el('p', { class: 'dato__titulo' }, 'Aciertos por sesión'),
    el('p', { class: 'dato__valor' }, `${primero} % → ${ultimo} %`),
    el('div', { class: 'avance__barras', role: 'img', 'aria-label': `Aciertos en Simón Dice: ${puntos.map((p) => `${p.pct} %`).join(', ')}` },
      puntos.map((p, i) => el('div', { class: 'avance__columna' },
        `${p.pct}%`,
        el('div', { class: `avance__barra${i === puntos.length - 1 ? ' avance__barra--ultima' : ''}`, style: { height: `${p.pct}%` } })))),
    el('div', { class: 'avance__fechas', 'aria-hidden': 'true' }, puntos.map((p) => el('span', {}, `${p.fecha.dia} ${p.fecha.mes}`))));
}

function pintarReparto(reparto) {
  const filas = Object.entries(reparto).sort((a, b) => b[1] - a[1]);
  if (!filas.length) {
    $('reparto').replaceChildren(el('p', { class: 'texto-ayuda' }, 'Sin datos todavía.'));
    return;
  }
  const maximo = filas[0][1];
  $('reparto').replaceChildren(...filas.map(([id, valor]) => el('div', { class: 'reparto' },
    cuadroCara(id, 44, 32),
    el('span', { class: 'reparto__nombre' }, datosAnimal(id).nombre),
    el('div', { class: 'reparto__pista', 'aria-hidden': 'true' },
      el('div', { class: 'reparto__barra', style: { width: `${(valor / maximo) * 100}%`, background: `var(--${id}-fuerte)` } })),
    el('span', { class: 'reparto__valor' }, String(valor)))));
}

async function cargarHistorial() {
  const alias = $('nino').value;
  if (!alias) {
    pintarSesiones([], 'nadie todavía');
    pintarAvance([]);
    pintarReparto({});
    return;
  }
  try {
    const datos = await obtenerHistorial(alias, $('periodo').value);
    pintarSesiones(datos.sesiones, alias);
    pintarAvance(datos.sesiones);
    pintarReparto(datos.reparto);
  } catch (error) {
    console.error('[historial] No se pudo cargar:', error);
    $('lista-sesiones').replaceChildren(el('p', { class: 'vacio' }, explicar(error, 'No se pudo cargar el historial')));
  }
}

async function arrancar() {
  iniciarPagina();
  try {
    ninos = await obtenerNinos();
    $('nino').replaceChildren(...ninos.map((n) => el('option', { value: n.alias }, n.nombre ? `${n.nombre} (${n.alias})` : n.alias)));
    // Si llegan desde la tarjeta de un niño (historial.html?alias=B-12), se abre el suyo
    const pedido = new URLSearchParams(location.search).get('alias');
    if (pedido && ninos.some((n) => n.alias === pedido)) $('nino').value = pedido;
  } catch (error) {
    console.error('[historial] No se pudo cargar la lista de niños:', error);
  }
  $('nino').addEventListener('change', cargarHistorial);
  $('periodo').addEventListener('change', cargarHistorial);
  cargarHistorial();
}

arrancar();
