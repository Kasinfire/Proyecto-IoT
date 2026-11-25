// Biblioteca de canciones con filtros. Toda la tarjeta es el botón para elegirla (ley de Fitts).
// Elegir una canción se lo pide al osito; se marca como "sonando" cuando el osito confirma.

import { el, icono, palomita, iniciarPagina } from './comun.js';
import { conectar } from './conexion.js';
import { borrarCancion, explicar, grabarGatito, obtenerCanciones, pedirModo } from './api.js';
import { abrirEditor, prepararEditor } from './editor-cancion.js';

const ESPERA_ACK_MS = 8000; // una canción propia primero se le manda al osito

const TIPOS = {
  tranquilas: { filtro: 'Tranquilas', tarjeta: 'Tranquila' },
  alegres: { filtro: 'Alegres', tarjeta: 'Alegre' },
  navidad: { filtro: 'Navidad', tarjeta: 'Navidad' },
  cumpleanos: { filtro: 'Cumpleaños', tarjeta: 'Cumpleaños' },
  bloques: { filtro: 'Bloques', tarjeta: 'Bloques' },
  mias: { filtro: 'Otras', tarjeta: 'Otra' },
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
    [['propias', 'Mis canciones', estado.canciones.filter((c) => !c.incluida).length]],
  ).filter(([id, , cuantas]) => cuantas || id === 'todas' || id === estado.filtro);
  $('filtros').replaceChildren(...opciones.map(([id, nombre, cuantas]) => el('button', {
    type: 'button', class: 'filtro', 'aria-pressed': String(estado.filtro === id),
    onclick: () => { estado.filtro = id; pintarFiltros(); pintarCanciones(); },
  }, `${nombre} `, el('span', {}, String(cuantas)))));
}

function pintarCanciones() {
  const visibles = estado.canciones.filter((c) => estado.filtro === 'todas' || c.tipo === estado.filtro || (estado.filtro === 'propias' && !c.incluida));
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

    const tarjeta = el('button', {
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
    const accion = (texto, hacer) => el('button', { type: 'button', class: 'cancion-caja__accion', 'aria-label': `${texto} ${c.nombre}`, onclick: hacer }, texto);
    return el('div', { class: 'cancion-caja' }, tarjeta, el('div', { class: 'cancion-caja__acciones' },
      c.incluida ? null : accion('Editar', () => abrirEditor(c, 'Editar canción')),
      accion('Duplicar', () => abrirEditor({ ...c, id: null, nombre: `${c.nombre} (copia)`.slice(0, 40) }, 'Nueva a partir de una copia')),
      c.incluida ? null : accion('Borrar', () => confirmarBorrar(c))));
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
    setTimeout(() => {
      if (estado.pedida === nombre) dejarDeEsperar('El osito no respondió. Revisa que esté encendido.');
    }, ESPERA_ACK_MS);
  } catch (error) {
    console.error('[canciones] No se pudo pedir la canción:', error);
    dejarDeEsperar(explicar(error, 'No se pudo cambiar la canción'));
  }
}

function dejarDeEsperar(texto) {
  estado.pedida = null;
  $('error').textContent = texto;
  $('error').hidden = false;
  pintarCanciones();
}

function alEvento(msg) {
  if (msg.tipo === 'canciones') {
    cargar();
    return;
  }
  if (msg.tipo === 'grabacion') {
    if (msg.activa) contarNotas(msg.notas);
    else terminarGrabacion();
    return;
  }
  if (msg.tipo === 'sesion' || (msg.tipo === 'modo' && msg.ok)) {
    estado.actual = msg.cancion ?? estado.actual;
    estado.modo = msg.modo ?? estado.modo;
    if (msg.tipo === 'modo') {
      estado.pedida = null;
      $('error').hidden = true;
    }
    pintarCanciones();
  } else if (msg.tipo === 'modo' && estado.pedida) {
    dejarDeEsperar('El osito no pudo cambiar a esa canción.');
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

let porBorrar = null;

function confirmarBorrar(c) {
  porBorrar = c;
  $('t-borrar').textContent = `¿Borrar «${c.nombre}»?`;
  $('borrar-error').hidden = true;
  $('ventana-borrar').showModal();
}

function prepararBorrar() {
  $('form-borrar').addEventListener('submit', async (e) => {
    if (e.submitter?.value !== 'borrar') return;
    e.preventDefault();
    try {
      await borrarCancion(porBorrar.id);
      $('ventana-borrar').close();
      await cargar();
    } catch (error) {
      console.error('[canciones] No se pudo borrar:', error);
      $('borrar-error').textContent = explicar(error, 'No se pudo borrar');
      $('borrar-error').hidden = false;
    }
  });
}

// ---------- Grabar con el gatito ----------

function contarNotas(n) {
  $('grabar-cuenta').textContent = n === 1 ? '1 nota' : `${n} notas`;
}

async function empezarGrabacion() {
  try {
    await grabarGatito('empezar');
    $('error').hidden = true;
    $('grabar-error').hidden = true;
    contarNotas(0);
    $('ventana-grabar').showModal();
  } catch (error) {
    console.error('[canciones] No se pudo grabar:', error);
    $('error').textContent = explicar(error, 'No se pudo grabar');
    $('error').hidden = false;
  }
}

async function terminarGrabacion() {
  if (!$('ventana-grabar').open) return;
  try {
    const { melodia, pulso_ms } = await grabarGatito('terminar');
    $('ventana-grabar').close('lista');
    abrirEditor({ melodia, pulso_ms, tipo: 'mias', estilo: 'suave', compas: 8, nombre: '' }, 'Canción grabada');
  } catch (error) {
    console.error('[canciones] No se pudo terminar:', error);
    $('ventana-grabar').close('lista');
    $('error').textContent = explicar(error, 'No se pudo grabar');
    $('error').hidden = false;
  }
}

function prepararGrabar() {
  $('grabar').addEventListener('click', empezarGrabacion);
  $('grabar-terminar').addEventListener('click', terminarGrabacion);
  $('grabar-cancelar').addEventListener('click', () => $('ventana-grabar').close('cancelar'));
  $('ventana-grabar').addEventListener('close', () => { // también con Esc
    if ($('ventana-grabar').returnValue !== 'lista') grabarGatito('cancelar').catch(() => {});
    $('ventana-grabar').returnValue = '';
  });
}

$('crear').addEventListener('click', () => abrirEditor(null, 'Crear canción'));
prepararEditor(cargar);
prepararBorrar();
prepararGrabar();
iniciarPagina();
cargar();
conectar({ alEvento });
