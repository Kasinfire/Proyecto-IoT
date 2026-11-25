// Niños del consultorio. Las sesiones van por alias; el nombre es opcional, solo se ve aquí y en la
// pantalla del terapeuta, y no sale de la laptop (Unidad 4, privacidad).

import { datosAnimal } from './caritas.js';
import { avatarAlAzar, avatarPorDefecto, cuadroAvatar } from './avatar.js';
import { crearEditorAvatar } from './editor-avatar.js';
import { el, icono, iniciarPagina } from './comun.js';
import { esEnsayo } from './conexion.js';
import { agregarNino, borrarNino, editarNino, explicar, iniciarSesion, obtenerNinos } from './api.js';

const $ = (id) => document.getElementById(id);
const conEnsayo = (ruta) => (esEnsayo() ? `${ruta}${ruta.includes('?') ? '&' : '?'}ensayo` : ruta);

function mostrarError(id, texto) {
  $(id).textContent = texto;
  $(id).hidden = false;
}

// Solo puede haber una sesión abierta a la vez.
async function empezar(alias) {
  try {
    await iniciarSesion(alias);
    location.href = conEnsayo('en-vivo.html');
  } catch (error) {
    console.error('[ninos] No se pudo iniciar la sesión:', error);
    mostrarError('error', explicar(error, 'No se pudo iniciar la sesión'));
  }
}

function pintar(ninos) {
  const haySesion = ninos.some((n) => n.en_sesion);
  if (!ninos.length) {
    $('ninos').replaceChildren(el('p', { class: 'vacio' }, 'Todavía no hay niños. Agrega el primero para poder iniciar una sesión.'));
    return;
  }
  $('ninos').replaceChildren(...ninos.map((n) => {
    const quien = n.nombre || n.alias;
    const favorito = n.favorito ? datosAnimal(n.favorito).nombre : null;
    const datos = [n.simon_pct === null ? null : `Simón ${n.simon_pct} %`, favorito ? `más: ${favorito}` : null].filter(Boolean).join(' · ');
    const accion = (dibujo, texto, hacer) => el('button', { type: 'button', class: 'nino__icono', 'aria-label': `${texto} a ${quien}`, title: texto, onclick: hacer }, icono(dibujo, 18));
    return el('article', { class: `nino${n.en_sesion ? ' nino--en-sesion' : ''}` },
      n.en_sesion ? el('span', { class: 'nino__etiqueta' }, 'en sesión') : null,
      cuadroAvatar(n.avatar, n.alias, 96),
      el('h2', { class: 'nino__alias' }, quien),
      el('p', { class: 'nino__sesiones' }, `${n.nombre ? `${n.alias} · ` : ''}${n.sesiones} ${n.sesiones === 1 ? 'sesión' : 'sesiones'}`),
      el('p', { class: 'nino__sesiones' }, datos || 'Sin datos todavía'),
      el('div', { class: 'nino__acciones' },
        n.en_sesion ? el('a', { class: 'boton boton--principal', href: conEnsayo('en-vivo.html') }, 'Ir a la sesión') : null,
        !haySesion ? el('button', { type: 'button', class: 'boton boton--principal', 'aria-label': `Iniciar sesión con ${quien}`, onclick: () => empezar(n.alias) }, 'Iniciar sesión') : null,
        el('div', { class: 'nino__iconos' },
          el('a', { class: 'nino__icono', href: conEnsayo(`historial.html?alias=${encodeURIComponent(n.alias)}`), 'aria-label': `Ver historial de ${quien}`, title: 'Ver historial' }, icono('reloj', 18)),
          accion('lapiz', 'Editar y vestir', () => abrirNino(n)),
          !n.en_sesion ? accion('basura', 'Borrar', () => abrirBorrar(n)) : null)));
  }));
}

async function cargar() {
  try {
    pintar(await obtenerNinos());
    $('error').hidden = true;
  } catch (error) {
    console.error('[ninos] No se pudo cargar la lista:', error);
    mostrarError('error', explicar(error, 'No se pudo cargar la lista de niños'));
  }
}

let editando = null; // el niño que se está editando, o null si es nuevo
let borrando = null;
const editor = crearEditorAvatar($('avatar-editor'));

function abrirNino(nino) {
  editando = nino;
  $('form-nino').reset();
  $('t-nino').textContent = nino ? `Editar a ${nino.nombre || nino.alias}` : 'Agregar niño';
  $('nino-guardar').textContent = nino ? 'Guardar' : 'Agregar';
  $('nombre').value = nino?.nombre ?? '';
  $('alias').value = nino?.alias ?? '';
  $('alias').required = Boolean(nino);
  $('alias-ayuda').textContent = nino
    ? 'Una o dos letras y un número, como A-07. Si lo cambias, su historial se conserva.'
    : 'Una o dos letras y un número, como A-07. Si lo dejas vacío se pone solo.';
  editor.poner(nino ? nino.avatar ?? avatarPorDefecto(nino.alias) : avatarAlAzar());
  $('nino-error').hidden = true;
  $('ventana-nino').showModal();
}

function abrirBorrar(nino) {
  borrando = nino;
  $('t-borrar').textContent = `¿Borrar a ${nino.nombre || nino.alias}?`;
  $('borrar-ayuda').textContent = nino.sesiones
    ? `También se ${nino.sesiones === 1 ? 'borra su sesión guardada' : `borran sus ${nino.sesiones} sesiones guardadas`}. No se puede deshacer.`
    : 'No tiene sesiones guardadas. No se puede deshacer.';
  $('borrar-error').hidden = true;
  $('ventana-borrar').showModal();
}

// Manda lo de una ventana; si falla, la deja abierta con la causa. Un envío a la vez.
function alEnviar(form, ventana, valor, idError, queFallo, accion) {
  let enviando = false;
  $(form).addEventListener('submit', async (e) => {
    if (e.submitter && e.submitter.value !== valor) return; // cancelar: el dialog se cierra solo
    e.preventDefault();
    if (enviando) return;
    enviando = true;
    try {
      await accion();
      $(ventana).close();
      await cargar();
    } catch (error) {
      console.error(`[ninos] ${queFallo}:`, error);
      mostrarError(idError, explicar(error, queFallo));
    } finally {
      enviando = false;
    }
  });
}

function prepararVentanas() {
  $('agregar').addEventListener('click', () => abrirNino(null));
  // Enter en un campo usaría el primer botón del formulario (Cancelar).
  $('form-nino').addEventListener('keydown', (e) => {
    if (e.key !== 'Enter' || !e.target.matches('.entrada') || e.isComposing) return;
    e.preventDefault();
    $('form-nino').requestSubmit($('nino-guardar'));
  });
  alEnviar('form-nino', 'ventana-nino', 'guardar', 'nino-error', 'No se pudo guardar', async () => {
    const alias = $('alias').value.trim().toUpperCase();
    const nombre = $('nombre').value.trim();
    if (editando) await editarNino(editando.alias, alias, nombre, editor.valor());
    else if (!alias && !nombre) throw Object.assign(new Error('vacío'), { causa: 'escribe un nombre o un alias' });
    else await agregarNino(alias, nombre, editor.valor());
  });
  alEnviar('form-borrar', 'ventana-borrar', 'borrar', 'borrar-error', 'No se pudo borrar', () => borrarNino(borrando.alias));
}

iniciarPagina();
prepararVentanas();
cargar();
