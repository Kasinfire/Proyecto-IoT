// Ventana para crear, editar o duplicar una canción: se arma la melodía nota por nota y el resto
// (bajo, acordes y ritmo) lo arregla Node-RED.

import { el } from './comun.js';
import { arreglarCancion, explicar, guardarCancion } from './api.js';
import { nota as sonarNota, parar, tocar } from './sonido.js';

const NOTAS = ['do', 'do#', 're', 're#', 'mi', 'fa', 'fa#', 'sol', 'sol#', 'la', 'la#', 'si'];
const BOTONES = [60, 62, 64, 65, 67, 69, 71, 72];
const DURACIONES = [[1, 'corta'], [2, 'media'], [4, 'larga']];
const PASOS_MAX = 127;
// "Más rápido" es un pulso más corto.
const PULSO = { lento: 420, rapido: 140 };

const $ = (id) => document.getElementById(id);
let melodia = [];
let elegida = null; // posición de la nota seleccionada
let dura = 2;
let id = null; // null = canción nueva
let alGuardar = () => {};

function nombreNota(midi) {
  if (midi === 0) return 'silencio';
  const octava = Math.floor(midi / 12) - 5;
  return NOTAS[midi % 12] + (octava > 0 ? '’'.repeat(octava) : ','.repeat(-octava));
}

const textoDura = (d) => `${d} ${d === 1 ? 'pulso' : 'pulsos'}`;

function actual() {
  const ritmo = $('ed-ritmo').value;
  return {
    id, nombre: $('ed-nombre').value.trim(), tipo: $('ed-tipo').value, melodia,
    estilo: ritmo.split('-')[0], compas: Number(ritmo.split('-')[1]),
    pulso_ms: PULSO.lento + PULSO.rapido - Number($('ed-velocidad').value),
  };
}

function pintar() {
  $('ed-melodia').replaceChildren(...(melodia.length ? melodia.map(([n, d], i) => el('button', {
    type: 'button', class: `ficha${n === 0 ? ' ficha--silencio' : ''}`, 'aria-pressed': String(elegida === i),
    'aria-label': `${i + 1}: ${nombreNota(n)}, ${textoDura(d)}`,
    style: { minWidth: `${40 + d * 8}px` },
    onclick: () => { elegida = elegida === i ? null : i; if (n) sonarNota(n); pintar(); $('ed-melodia').children[i]?.focus(); },
  }, el('strong', {}, n === 0 ? '–' : nombreNota(n)), el('small', {}, String(d)))) : [el('p', { class: 'texto-ayuda' }, 'Toca las notas de abajo para armar la melodía.')]));
  $('ed-duraciones').replaceChildren(...DURACIONES.map(([d, t]) => el('button', {
    type: 'button', class: 'filtro', 'aria-pressed': String(dura === d),
    onclick: () => { dura = d; if (elegida !== null) melodia[elegida][1] = d; pintar(); },
  }, `${t} (${d})`)));
  const total = melodia.reduce((t, [, d]) => t + d, 0);
  $('ed-cuenta').textContent = elegida === null
    ? `${melodia.length} notas · ${Math.max(0, Math.round((total * actual().pulso_ms) / 1000))} s`
    : `Nota ${elegida + 1} elegida: las teclas la cambian`;
  $('ed-quitar').disabled = !melodia.length;
  $('ed-quitar').textContent = elegida === null ? 'Quitar la última' : 'Quitar la elegida';
  $('ed-vaciar').disabled = !melodia.length;
}

function mostrarError(texto) {
  $('ed-error').textContent = texto;
  $('ed-error').hidden = !texto;
}

function poner(midi) {
  if (elegida !== null) melodia[elegida] = [midi, melodia[elegida][1]];
  else if (melodia.length >= PASOS_MAX) { mostrarError(`La melodía puede tener hasta ${PASOS_MAX} notas.`); return; } else melodia.push([midi, dura]);
  mostrarError('');
  if (midi) sonarNota(midi);
  pintar();
  if (elegida === null) $('ed-melodia').scrollTop = $('ed-melodia').scrollHeight;
}

async function oir() {
  if ($('ed-oir').dataset.sonando) { parar(); return; }
  try {
    const { partes, pulso_ms: pulso } = await arreglarCancion(actual());
    mostrarError('');
    $('ed-oir').dataset.sonando = '1';
    $('ed-oir').textContent = 'Parar';
    await tocar(partes, pulso, () => { delete $('ed-oir').dataset.sonando; $('ed-oir').textContent = 'Oír'; });
  } catch (error) {
    console.error('[editor] No se pudo oír:', error);
    mostrarError(explicar(error, 'No se pudo oír'));
  }
}

// cancion: null = nueva; con id = editar; sin id = copia o grabación.
export function abrirEditor(cancion, titulo) {
  id = cancion?.id ?? null;
  melodia = (cancion?.melodia ?? []).map((p) => [...p]);
  elegida = null;
  dura = 2;
  $('t-editor').textContent = titulo;
  $('ed-nombre').value = cancion?.nombre ?? '';
  $('ed-tipo').value = cancion?.tipo ?? 'alegres';
  $('ed-ritmo').value = cancion?.estilo ? `${cancion.estilo}-${cancion.compas}` : 'marcha-8';
  $('ed-velocidad').value = PULSO.lento + PULSO.rapido - Math.min(PULSO.lento, Math.max(PULSO.rapido, cancion?.pulso_ms ?? 250));
  mostrarError('');
  pintar();
  $('ventana-editor').showModal();
}

export function prepararEditor(guardado) {
  alGuardar = guardado;
  $('ed-notas').replaceChildren(...BOTONES.map((m) => el('button', { type: 'button', class: 'nota-boton', onclick: () => poner(m) }, nombreNota(m))),
    el('button', { type: 'button', class: 'nota-boton nota-boton--silencio', onclick: () => poner(0) }, 'silencio'));
  $('ed-quitar').addEventListener('click', () => { melodia.splice(elegida ?? melodia.length - 1, 1); elegida = null; pintar(); });
  $('ed-vaciar').addEventListener('click', () => { melodia = []; elegida = null; pintar(); });
  $('ed-velocidad').addEventListener('input', pintar);
  $('ed-oir').addEventListener('click', oir);
  $('ventana-editor').addEventListener('close', parar);
  $('ventana-editor').addEventListener('cancel', (e) => {
    // Esc no debe tirar una melodía a medias.
    if (melodia.length) e.preventDefault();
  });
  $('form-editor').addEventListener('keydown', (e) => {
    if (e.key !== 'Enter' || e.target !== $('ed-nombre') || e.isComposing) return;
    e.preventDefault(); // usaría el primer botón del formulario
    $('form-editor').requestSubmit($('ed-guardar'));
  });
  let enviando = false;
  $('form-editor').addEventListener('submit', async (e) => {
    if (e.submitter?.value === 'cancelar') return;
    e.preventDefault();
    if (enviando) return;
    enviando = true;
    try {
      await guardarCancion(actual());
      $('ventana-editor').close();
      await alGuardar();
    } catch (error) {
      console.error('[editor] No se pudo guardar:', error);
      mostrarError(explicar(error, 'No se pudo guardar'));
    } finally {
      enviando = false;
    }
  });
}
