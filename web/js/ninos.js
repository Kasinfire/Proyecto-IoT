// Niños del consultorio, siempre por alias (nunca el nombre real: Unidad 4, privacidad).

import { cuadroCara, datosAnimal } from './caritas.js';
import { el, iniciarPagina } from './comun.js';
import { esEnsayo } from './conexion.js';
import { agregarNino, obtenerNinos } from './api.js';

const $ = (id) => document.getElementById(id);
const conEnsayo = (ruta) => (esEnsayo() ? `${ruta}${ruta.includes('?') ? '&' : '?'}ensayo` : ruta);

function pintar(ninos) {
  $('ninos').replaceChildren(...ninos.map((n) => {
    const favorito = n.favorito ? datosAnimal(n.favorito) : null;
    return el('article', { class: `nino${n.en_sesion ? ' nino--en-sesion' : ''}` },
      el('div', { class: 'nino__cabeza' },
        favorito
          ? cuadroCara(favorito.id, 64, 44, n.en_sesion ? '#ffffff' : `var(--${favorito.id}-claro)`)
          : el('div', { class: 'cuadro-cara', style: { width: '64px', height: '64px', background: '#ffffff' } }),
        el('div', {},
          el('h2', { class: 'nino__alias' }, n.alias),
          el('p', { class: 'nino__sesiones' }, `${n.sesiones} ${n.sesiones === 1 ? 'sesión' : 'sesiones'}`)),
        n.en_sesion ? el('span', { class: 'nino__etiqueta' }, 'en sesión') : null),
      el('div', { class: 'nino__datos' },
        el('div', { class: 'mini-dato' }, el('small', {}, 'Simón Dice'), el('strong', {}, n.simon_pct === null ? '—' : `${n.simon_pct} %`)),
        el('div', { class: 'mini-dato' }, el('small', {}, 'Toca más'), el('strong', {}, favorito ? favorito.nombre : '—'))),
      el('div', { class: 'nino__acciones' },
        n.en_sesion ? el('a', { class: 'boton boton--principal', href: conEnsayo('en-vivo.html') }, 'Ir a la sesión') : null,
        el('a', { class: 'boton boton--blanco', href: conEnsayo(`historial.html?alias=${encodeURIComponent(n.alias)}`) }, 'Ver historial')));
  }));
}

async function cargar() {
  try {
    pintar(await obtenerNinos());
    $('error').hidden = true;
  } catch (error) {
    console.error('[ninos] No se pudo cargar la lista:', error);
    $('error').textContent = 'No se pudo cargar la lista de niños. Revisa que Node-RED esté encendido.';
    $('error').hidden = false;
  }
}

function prepararAgregar() {
  const ventana = $('ventana-agregar');
  $('agregar').addEventListener('click', () => {
    $('form-agregar').reset();
    $('agregar-error').hidden = true;
    ventana.showModal();
  });
  $('form-agregar').addEventListener('submit', async (e) => {
    if (e.submitter?.value !== 'agregar') return;
    e.preventDefault();
    const alias = $('alias').value.trim().toUpperCase();
    try {
      await agregarNino(alias);
      ventana.close();
      cargar();
    } catch (error) {
      console.error('[ninos] No se pudo agregar:', error);
      $('agregar-error').textContent = 'No se pudo guardar. Intenta otra vez.';
      $('agregar-error').hidden = false;
    }
  });
}

iniciarPagina();
prepararAgregar();
cargar();
