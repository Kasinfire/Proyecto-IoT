// Biblioteca de canciones con filtros. Toda la tarjeta es el botón para elegirla (ley de Fitts).
// Elegir una canción se lo pide al osito; se marca como "sonando" cuando el osito confirma.

import { el, icono, palomita, iniciarPagina } from './comun.js';
import { conectar } from './conexion.js';
import { obtenerCanciones, pedirModo } from './api.js';

const TIPOS = {
  tranquilas: { filtro: 'Tranquilas', tarjeta: 'Tranquila' },
  alegres: { filtro: 'Alegres', tarjeta: 'Alegre' },
  navidad: { filtro: 'Navidad', tarjeta: 'Navidad' },
  cumpleanos: { filtro: 'Cumpleaños', tarjeta: 'Cumpleaños' },
};

const estado = {
  canciones: [],
  filtro: 'todas',
  actual: null,      // la que confirmó el osito
  pedida: null,      // la que se pidió y todavía no confirma
  modo: 'cancion',
};

const $ = (id) => document.getElementById(id);

function pintarFiltros() {
  const opciones = [['todas', 'Todas', estado.canciones.length]].concat(
    Object.entries(TIPOS).map(([id, t]) => [id, t.filtro, estado.canciones.filter((c) => c.tipo === id).length]),
  );
  $('filtros').replaceChildren(...opciones.map(([id, nombre, cuantas]) => el('button', {
    type: 'button', class: 'filtro', 'aria-pressed': String(estado.filtro === id),
    onclick: () => { estado.filtro = id; pintarFiltros(); pintarCanciones(); },
  }, `${nombre} `, el('span', {}, String(cuantas)))));
}

function pintarCanciones() {
  const visibles = estado.canciones.filter((c) => estado.filtro === 'todas' || c.tipo === estado.filtro);
  $('canciones').replaceChildren(...visibles.map((c) => {
    const sonando = c.nombre === estado.actual;
    const pidiendo = c.nombre === estado.pedida;
    let marca;
    if (sonando) marca = palomita(32);
    else if (pidiendo) marca = el('span', { class: 'cancion__elegir' }, icono('reloj', 16, 2.4));
    else marca = el('span', { class: 'cancion__elegir' }, triangulo());

    let pie = `${TIPOS[c.tipo]?.tarjeta ?? ''} · ${c.duracion}`;
    if (sonando) pie = `Sonando · ${c.duracion}`;
    if (pidiendo) pie = 'Enviando al osito…';

    return el('button', {
      type: 'button',
      class: `cancion cancion--${c.tipo}`,
      'aria-pressed': String(sonando),
      'aria-label': sonando ? `${c.nombre}, sonando ahora` : `Usar ${c.nombre} en esta sesión`,
      onclick: () => elegir(c.nombre),
    },
    el('span', { class: 'cancion__arriba' }, el('span', { class: 'cancion__icono' }, icono('nota', 20)), marca),
    el('span', { class: 'cancion__abajo' },
      el('span', { class: 'cancion__nombre', style: { display: 'block' } }, c.nombre),
      el('span', { class: 'cancion__pie' }, pie)));
  }));
}

// Triangulito de "reproducir" relleno (no es un ícono de línea)
function triangulo() {
  const svg = icono('nota', 12);
  svg.setAttribute('fill', '#1c1c1a');
  svg.setAttribute('stroke', 'none');
  svg.innerHTML = '<path d="M7 4l13 8-13 8z"></path>';
  return svg;
}

async function elegir(nombre) {
  if (nombre === estado.actual || estado.pedida) return;
  estado.pedida = nombre;
  pintarCanciones();
  try {
    // Elegir canción deja al osito en modo Canción
    await pedirModo('cancion', nombre);
  } catch (error) {
    console.error('[canciones] No se pudo pedir la canción:', error);
    estado.pedida = null;
    pintarCanciones();
  }
}

function alEvento(msg) {
  if (msg.tipo === 'sesion' || (msg.tipo === 'modo' && msg.ok)) {
    estado.actual = msg.cancion ?? estado.actual;
    estado.modo = msg.modo ?? estado.modo;
    if (msg.tipo === 'modo') estado.pedida = null;
    pintarCanciones();
  }
}

async function cargar() {
  try {
    estado.canciones = await obtenerCanciones();
    pintarFiltros();
    pintarCanciones();
  } catch (error) {
    console.error('[canciones] No se pudieron cargar:', error);
    $('error').textContent = 'No se pudieron cargar las canciones. Revisa que Node-RED esté encendido.';
    $('error').hidden = false;
  }
}

iniciarPagina();
cargar();
conectar({ alEvento });
