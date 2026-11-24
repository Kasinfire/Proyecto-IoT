// Sesión en vivo del terapeuta: animalitos con su partitura, juego, conexión y resumen.
// Todo lo que se ve sale de los eventos que manda Node-RED (o el ensayo).

import { ANIMALES, cara, cuadroCara } from './caritas.js';
import { el, icono, palomita, haceCuanto, iniciarPagina } from './comun.js';
import { conectar, esEnsayo, simulador } from './conexion.js';
import { crearPanel } from './panel.js';
import { explicar, obtenerCanciones, obtenerNinos, pedirModo, silenciarAviso, terminarSesion } from './api.js';

const ACTIVO_MS = 2000;           // un animal está "tocando" si tuvo un gesto hace menos de esto
const CELDAS = 20;                // segundos que muestra la partitura
const ESPERA_ACK_MS = 8000;       // si el osito no confirma en este tiempo, se avisa
const RECUPERADO_VISIBLE_MS = 6000;
const MINIMO_JUNTOS = 3;          // "tocando juntos" = 3 o más animales a la vez

const MODOS = [
  { id: 'cancion', nombre: 'Canción' },
  { id: 'libre', nombre: 'Libre' },
  { id: 'simon', nombre: 'Simón Dice' },
];

// ---------- Estado de la sesión ----------

const estado = {
  alias: null,
  nombre: '',               // opcional; solo lo ve el terapeuta
  inicio: null,
  modo: 'cancion',
  cancion: null,
  pendiente: null,          // { id, modo, cancion, desde } mientras el osito no confirma
  sinRespuesta: false,
  ws: 'conectando',
  internet: true,
  pendientesOsito: 0,
  guardado: null,
  recuperados: 0,
  recuperadoHasta: 0,
  silenciado: false,
  alertaCerrada: '',        // animales del aviso que se cerró a mano
  gestos: { gatito: 0, ranita: 0, pollito: 0 },
  simon: { aciertos: 0, turnos: 0 },
  juntosS: 0,
  animales: Object.fromEntries(ANIMALES.map((a) => [a.id, {
    conectado: null,        // null = todavía no se sabe
    desconectadoDesde: null,
    ultimoGesto: 0,
    detalle: null,
    sombra: 0,
    celdas: Array(CELDAS).fill(null),
  }])),
};

// ---------- Eventos que llegan ----------

function marcarGesto(id, texto = '') {
  const a = estado.animales[id];
  a.ultimoGesto = Date.now();
  a.celdas[CELDAS - 1] = { tipo: 'toque', texto };
}

function alEvento(msg) {
  const a = estado.animales[msg.nodo];
  switch (msg.tipo) {
    case 'sesion':
      estado.alias = msg.alias;
      estado.inicio = msg.inicio;
      estado.modo = msg.modo ?? estado.modo;
      estado.cancion = msg.cancion ?? estado.cancion;
      if (msg.resumen) {
        Object.assign(estado.gestos, msg.resumen.gestos);
        Object.assign(estado.simon, msg.resumen.simon);
        estado.juntosS = msg.resumen.juntos_s ?? estado.juntosS;
      }
      if (msg.silenciado !== undefined) estado.silenciado = msg.silenciado;
      pintarSilencio();
      pintarTitulo();
      buscarNombre();
      pintarModos();
      pintarCancionElegida();
      break;
    case 'estado':
      if (!a) return;
      if (a.conectado !== false && msg.conectado === false) a.desconectadoDesde = Date.now();
      a.conectado = msg.conectado;
      break;
    case 'nota':
      if (!a || msg.vel === 0) return; // vel 0 = soltó la tecla
      estado.gestos.gatito += 1;
      a.detalle = { texto: `Última nota: ${msg.nota}`, desde: Date.now() };
      marcarGesto('gatito', msg.nota);
      break;
    case 'sombra':
      if (!a) return;
      if (msg.nivel > 0) {
        if (a.sombra === 0) estado.gestos.ranita += 1;
        marcarGesto('ranita');
      }
      a.sombra = msg.nivel;
      a.detalle = { texto: msg.nivel > 0 ? `Mano tapando ${msg.nivel} %` : 'Sin mano encima' };
      break;
    case 'agitar':
      if (!a) return;
      estado.gestos.pollito += 1;
      a.detalle = { texto: `Sacudida de ${msg.intensidad} %` };
      marcarGesto('pollito');
      break;
    case 'panel':
      panel.actualizar(msg);
      return;
    case 'tecla':
      panel.destellar(msg.tecla);
      return;
    case 'canciones':
      cargarCanciones();
      return;
    case 'cancion':
    case 'grabacion':
      return;
    case 'volumen':
      estado.animales.osito.detalle = { texto: `Volumen ${msg.valor} %` };
      break;
    case 'modo':
      // Confirmación del osito (Request/Response). También llega si cambiaron el modo con su botón.
      if (msg.ok) {
        estado.modo = msg.modo;
        estado.cancion = msg.cancion ?? estado.cancion;
      }
      if (estado.pendiente && msg.id === estado.pendiente.id) estado.pendiente = null;
      estado.sinRespuesta = false;
      pintarModos();
      pintarCancionElegida();
      break;
    case 'simon':
      if (msg.resultado) {
        estado.simon.turnos += 1;
        if (msg.resultado === 'acierto') estado.simon.aciertos += 1;
      }
      break;
    case 'red': {
      const volvio = !estado.internet && msg.internet;
      estado.internet = msg.internet;
      estado.pendientesOsito = msg.pendientes ?? 0;
      if (msg.guardado) estado.guardado = msg.guardado;
      if (volvio) {
        estado.recuperados = msg.recuperados ?? 0;
        estado.recuperadoHasta = Date.now() + RECUPERADO_VISIBLE_MS;
      }
      pintarConexion();
      break;
    }
    default:
      break;
  }
  pintarFilas();
  pintarAlerta();
}

// ---------- Cálculos ----------

function estaActivo(id) {
  const a = estado.animales[id];
  if (a.conectado === false) return false;
  if (id === 'osito') return a.conectado === true; // la base del osito siempre suena
  return Date.now() - a.ultimoGesto < ACTIVO_MS;
}

function situacion(id) {
  const a = estado.animales[id];
  if (a.conectado === false) return 'fuera';
  return estaActivo(id) ? 'activo' : 'espera';
}

function textoDetalle(id) {
  const a = estado.animales[id];
  if (a.conectado === false) return `Sin señal ${haceCuanto(Date.now() - a.desconectadoDesde)}`;
  if (id === 'gatito' && a.detalle) return `${a.detalle.texto}, ${haceCuanto(Date.now() - a.detalle.desde)}`;
  if (a.detalle) return a.detalle.texto;
  return { osito: 'Dirige la orquesta', ranita: 'Sin mano encima', gatito: 'Aún sin notas', pollito: 'Aún sin sacudidas' }[id];
}

// Cada segundo la partitura avanza una celda.
function avanzarSegundo() {
  let activos = 0;
  for (const { id } of ANIMALES) {
    const a = estado.animales[id];
    a.celdas.shift();
    let nueva = null;
    if (a.conectado === false) nueva = { tipo: 'sin' };
    else if (id === 'osito' && a.conectado) nueva = { tipo: 'toque' };
    else if (id === 'ranita' && a.sombra > 0) nueva = { tipo: 'toque' };
    a.celdas.push(nueva);
    if (estaActivo(id)) activos += 1;
  }
  if (activos >= MINIMO_JUNTOS) estado.juntosS += 1;
}

// ---------- Pintar ----------

const $ = (id) => document.getElementById(id);

function pintarTitulo() {
  $('titulo').textContent = estado.alias ? `Sesión con ${quien()}` : 'Sin sesión abierta';
  // Sin sesión no hay nada que terminar; se abre desde Niños.
  $('terminar').hidden = !estado.alias;
  $('iniciar').hidden = Boolean(estado.alias);
}

function quien() {
  return estado.nombre ? `${estado.nombre} (${estado.alias})` : estado.alias;
}

async function buscarNombre() {
  const alias = estado.alias;
  estado.nombre = '';
  if (!alias) return;
  try {
    const nino = (await obtenerNinos()).find((n) => n.alias === alias);
    if (estado.alias !== alias) return; // cambió mientras tanto
    estado.nombre = nino?.nombre ?? '';
    pintarTitulo();
  } catch (error) {
    console.warn('[en-vivo] No se pudo leer el nombre:', error);
  }
}

function pintarSilencio() {
  $('silenciar').setAttribute('aria-pressed', String(estado.silenciado));
  $('silenciar').textContent = estado.silenciado ? 'Reactivar sonido' : 'Silenciar sonido';
}

function pintarFilas() {
  const tocando = ANIMALES.filter(({ id }) => estaActivo(id)).length;
  $('resumen-animales').textContent = `· ${tocando} de ${ANIMALES.length} tocando`;

  $('filas').replaceChildren(...ANIMALES.map(({ id, nombre, parte }) => {
    const sit = situacion(id);
    const etiqueta = { activo: 'Tocando', espera: 'En espera', fuera: 'Desconectado' }[sit];
    const iconoEstado = { activo: 'onda', espera: 'linea', fuera: 'tache' }[sit];
    const celdas = estado.animales[id].celdas;
    const tocadas = celdas.filter((c) => c?.tipo === 'toque').length;

    // Cada fila siempre lleva el color de su animal; el estado lo dice la etiqueta, no el fondo.
    return el('article', { class: `fila-animal fila-animal--${sit}`, style: `--color-animal: var(--${id}-fuerte); background: var(--${id}-claro)` },
      el('div', { class: 'fila-animal__quien' },
        cuadroCara(id, 46, 34, '#ffffff'),
        el('div', {},
          el('h3', { class: 'fila-animal__nombre' }, nombre),
          el('p', { class: 'fila-animal__parte' }, parte))),
      el('div', { class: 'fila-animal__estado' },
        el('span', { class: `estado estado--${sit}` }, icono(iconoEstado, 14, 2.6), etiqueta),
        el('span', { class: 'fila-animal__detalle' }, textoDetalle(id))),
      el('div', { class: 'partitura', role: 'img', 'aria-label': `${nombre}: tocó en ${tocadas} de los últimos ${CELDAS} segundos` },
        celdas.map((c) => {
          if (c?.tipo === 'sin') return el('span', { class: 'celda celda--sin' }, '×');
          if (c?.tipo !== 'toque') return el('span', { class: 'celda' });
          return el('span', { class: 'celda celda--toque' }, c.texto ?? '');
        })));
  }));
}

function pintarAlerta() {
  const fuera = ANIMALES.filter(({ id }) => estado.animales[id].conectado === false);
  const alerta = $('alerta');
  // Si la terapeuta cerró el aviso, no vuelve a salir hasta que se desconecte alguien más
  const clave = fuera.map((a) => a.id).join(',');
  if (!fuera.length || clave === estado.alertaCerrada) {
    if (!fuera.length) estado.alertaCerrada = '';
    alerta.hidden = true;
    return;
  }
  const primero = fuera[0];
  const desde = estado.animales[primero.id].desconectadoDesde;
  $('alerta-cara').replaceChildren(cara(primero.id, 34));
  $('alerta-titulo').textContent = fuera.length === 1
    ? `${primero.id === 'ranita' ? 'La' : 'El'} ${primero.nombre} se desconectó ${haceCuanto(Date.now() - desde)}`
    : `Se desconectaron: ${fuera.map((a) => a.nombre).join(' y ')}`;
  $('alerta-ayuda').textContent = primero.id === 'osito'
    ? 'Revisa que el osito esté encendido y conectado a la laptop.'
    : 'Revisa que esté encendido y cerca del osito. El LED rojo del osito también avisa.';
  alerta.hidden = false;
}

function pintarModos() {
  panel.modo(estado.modo);
  $('modos').replaceChildren(...MODOS.map(({ id, nombre }) => {
    const elegido = estado.pendiente ? estado.pendiente.modo === id : estado.modo === id;
    let sello = null;
    if (elegido && estado.pendiente) {
      sello = el('span', { class: 'modo__sello' }, icono('reloj', 18, 2.4), 'enviando…');
    } else if (elegido && estado.sinRespuesta) {
      sello = el('span', { class: 'modo__sello' }, icono('tache', 16, 2.6), 'sin respuesta');
    } else if (elegido) {
      sello = el('span', { class: 'modo__sello' }, palomita(), el('span', { class: 'solo-lector' }, 'confirmado por el osito'));
    }
    return el('button', {
      type: 'button', class: 'modo', role: 'radio', 'aria-checked': String(elegido),
      onclick: () => elegirModo(id, estado.cancion),
    }, el('span', { class: 'modo__circulo', 'aria-hidden': 'true' }), el('span', { class: 'modo__nombre' }, nombre), sello);
  }));
}

function pintarCancionElegida() {
  if (estado.cancion) $('cancion').value = estado.cancion;
}

let claveConexion = '';
function pintarConexion() {
  panel.conexion(estado.ws === 'ensayo' || (estado.ws === 'conectado' && estado.internet && estado.animales.osito.conectado === true));
  const ahora = Date.now();
  let tipo;
  let valor;
  let ayuda;
  let sinRed = false;

  if (estado.ws === 'sin-conexion' || estado.ws === 'conectando') {
    tipo = 'aviso'; sinRed = true;
    valor = 'Sin conexión con Node-RED';
    ayuda = 'Reintentando… la música del osito no se detiene';
  } else if (!estado.internet) {
    tipo = 'aviso'; sinRed = true;
    valor = 'El osito no está conectado';
    ayuda = `La música sigue sonando. Guarda hasta 64 eventos y los manda al volver (tenía ${estado.pendientesOsito}).`;
  } else if (ahora < estado.recuperadoHasta) {
    tipo = 'bien';
    valor = 'El osito está de vuelta';
    ayuda = `Traía ${estado.recuperados} eventos guardados`;
  } else {
    tipo = 'bien';
    valor = 'Broker y osito en línea';
    ayuda = `${estado.pendientesOsito} eventos pendientes${estado.guardado ? ` · guardado ${haceCuanto(ahora - estado.guardado)}` : ''}`;
  }

  // Solo se vuelve a dibujar si cambió, para no perder el foco del botón.
  const clave = [tipo, valor, ayuda].join('|');
  if (clave === claveConexion) return;
  claveConexion = clave;

  const tarjeta = el('div', { class: `conexion conexion--${tipo}` },
    el('div', { class: 'conexion__fila' },
      el('div', { class: 'conexion__icono' }, icono(sinRed ? 'wifiTachado' : 'wifi', 20)),
      el('div', {}, el('p', { class: 'conexion__valor' }, valor), el('p', { class: 'conexion__ayuda' }, ayuda))));

  // El botón para simular la caída solo existe en ensayo: en la demo real se apaga el router.
  if (esEnsayo()) {
    const caida = !estado.internet;
    tarjeta.append(el('button', {
      type: 'button', class: `boton ${caida ? 'boton--aviso' : 'boton--bien'}`,
      onclick: () => simulador()?.simularCaida(!caida),
    }, icono(caida ? 'reintentar' : 'onda', 16, 2.4), caida ? 'Reconectar' : 'Simular caída'));
  }
  $('conexion').replaceChildren(tarjeta);
}

function datosResumen() {
  const g = estado.gestos;
  const total = g.gatito + g.ranita + g.pollito;
  const minutos = estado.inicio ? Math.floor((Date.now() - estado.inicio) / 60000) : 0;
  const desde = estado.inicio
    ? new Date(estado.inicio).toLocaleTimeString('es-MX', { hour: '2-digit', minute: '2-digit' })
    : '—';
  return [
    { titulo: 'Duración', valor: `${minutos} min`, ayuda: `desde las ${desde}`, fondo: 'var(--lima)' },
    { titulo: 'Gestos', valor: String(total), ayuda: `Gatito ${g.gatito} · Ranita ${g.ranita} · Pollito ${g.pollito}`, fondo: 'var(--gatito-claro)' },
    { titulo: 'Simón Dice', valor: `${estado.simon.aciertos} de ${estado.simon.turnos}`, ayuda: 'turnos acertados', fondo: 'var(--amarillo)' },
    { titulo: 'Tocando juntos', valor: `${Math.floor(estado.juntosS / 60)} min`, ayuda: `${MINIMO_JUNTOS} o más animales a la vez`, fondo: 'var(--osito-claro)' },
  ];
}

function pintarResumen() {
  $('resumen').replaceChildren(...datosResumen().map((d) => el('div', { class: 'dato', style: { background: d.fondo } },
    el('p', { class: 'dato__titulo' }, d.titulo),
    el('p', { class: 'dato__valor' }, d.valor),
    el('p', { class: 'dato__ayuda' }, d.ayuda))));
}

// ---------- Acciones del terapeuta ----------

async function elegirModo(modo, cancion) {
  if (estado.pendiente) return; // una petición a la vez
  if (modo === estado.modo && cancion === estado.cancion) return;
  estado.pendiente = { id: null, modo, cancion, desde: Date.now() };
  estado.sinRespuesta = false;
  pintarModos();
  try {
    const { id } = await pedirModo(modo, cancion);
    if (estado.pendiente) estado.pendiente.id = id;
  } catch (error) {
    console.error('[en-vivo] No se pudo pedir el cambio de modo:', error);
    estado.pendiente = null;
    estado.sinRespuesta = true;
    pintarModos();
  }
}

function revisarEsperaDelOsito() {
  if (estado.pendiente && Date.now() - estado.pendiente.desde > ESPERA_ACK_MS) {
    estado.pendiente = null;
    estado.sinRespuesta = true;
    pintarModos();
    pintarCancionElegida();
  }
}

async function cargarCanciones() {
  try {
    const canciones = await obtenerCanciones();
    $('cancion').replaceChildren(...canciones.map((c) => el('option', { value: c.nombre }, c.nombre)));
    pintarCancionElegida();
  } catch (error) {
    console.error('[en-vivo] No se pudieron cargar las canciones:', error);
  }
}

function prepararAcciones() {
  $('cancion').addEventListener('change', (e) => elegirModo(estado.modo, e.target.value));

  $('cerrar-alerta').addEventListener('click', () => {
    estado.alertaCerrada = ANIMALES.filter(({ id }) => estado.animales[id].conectado === false).map((a) => a.id).join(',');
    $('alerta').hidden = true;
  });

  $('silenciar').addEventListener('click', async () => {
    estado.silenciado = !estado.silenciado;
    pintarSilencio();
    try {
      await silenciarAviso(estado.silenciado);
    } catch (error) {
      console.error('[en-vivo] No se pudo cambiar el sonido de aviso:', error);
      estado.silenciado = !estado.silenciado; // no se aplicó
      pintarSilencio();
    }
  });

  // Terminar pide confirmación: es una acción que no se puede deshacer.
  const ventana = $('ventana-terminar');
  $('terminar').addEventListener('click', () => {
    const [duracion, gestos, simon] = datosResumen();
    $('t-terminar').textContent = estado.alias ? `¿Terminar la sesión con ${quien()}?` : '¿Terminar la sesión?';
    $('terminar-datos').replaceChildren(...[duracion, gestos, simon].map((d) => el('div', { class: 'ventana__dato', style: { background: d.fondo } },
      el('small', {}, d.titulo), el('strong', {}, d.valor))));
    $('terminar-error').hidden = true;
    ventana.showModal();
  });
  $('form-terminar').addEventListener('submit', async (e) => {
    if (e.submitter?.value !== 'terminar') return;
    e.preventDefault();
    try {
      await terminarSesion($('nota').value.trim());
      location.href = `historial.html${esEnsayo() ? '?ensayo' : ''}`;
    } catch (error) {
      console.error('[en-vivo] No se pudo terminar la sesión:', error);
      $('terminar-error').textContent = explicar(error, 'No se pudo guardar');
      $('terminar-error').hidden = false;
    }
  });
}

// ---------- Arranque ----------

const panel = crearPanel($('panel'));
iniciarPagina();
prepararAcciones();
cargarCanciones();
pintarModos();
pintarFilas();
pintarConexion();
pintarResumen();

conectar({
  alEvento,
  alEstado: (e) => {
    estado.ws = e;
    pintarConexion();
  },
});

setInterval(() => {
  avanzarSegundo();
  revisarEsperaDelOsito();
  pintarFilas();
  pintarAlerta();
  pintarConexion();
  pintarResumen();
}, 1000);

// Entre segundos solo se refresca quién está tocando, para que se sienta inmediato.
setInterval(pintarFilas, 250);
