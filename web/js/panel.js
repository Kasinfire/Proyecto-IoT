// Panel DJ: el teclado 4x4 del osito en la pantalla. Se ilumina con las teclas reales, muestra qué
// está encendido y también se puede tocar desde aquí (Node-RED se lo manda al osito).

import { cara } from './caritas.js';
import { el, icono } from './comun.js';
import { explicar, tocarTecla } from './api.js';

const TECLAS = [...'123A456B789C*0#D'];
const DESTELLO_MS = 250;
const LARGA_MS = 1000;
const ESPERA_MS = 8000; // sin respuesta del osito en este tiempo, se avisa
const RITMOS = { apagado: 'apagado', tranquilo: 'tranquilo', pop: 'pop', cumbia: 'cumbia' };
const LOOPS = { vacio: 'vacío', grabando: 'grabando', sonando: 'sonando', parado: 'en pausa' };
const ANIMAL = { 1: ['ranita', 'Ranita'], 2: ['gatito', 'Gatito'], 3: ['pollito', 'Pollito'] };
const MODO = { A: ['cancion', 'Canción'], B: ['libre', 'Libre'], C: ['simon', 'Simón Dice'] };

// { texto, estado (palabra corta o null), encendida, sirve }
function describir(tecla, p, modo) {
  if (ANIMAL[tecla]) {
    const callado = p.callados.includes(ANIMAL[tecla][0]);
    return { texto: ANIMAL[tecla][1], estado: callado ? 'en silencio' : 'suena', encendida: !callado, sirve: true };
  }
  if (MODO[tecla]) {
    const actual = modo === MODO[tecla][0];
    return { texto: MODO[tecla][1], estado: actual ? 'modo actual' : null, encendida: actual, sirve: true };
  }
  const simple = (texto, estado = null, encendida = false) => ({ texto, estado, encendida, sirve: true });
  if (modo === 'simon') {
    return {
      7: simple('Bajar nivel'), 9: simple('Subir nivel', `nivel ${p.nivel}`), '*': simple('Empezar de nuevo'),
      '#': simple('Repetir reto'), 0: simple('Dar pista'),
    }[tecla] ?? { texto: 'Sin uso', estado: null, encendida: false, sirve: false };
  }
  return {
    4: simple('Eco', p.eco ? 'encendido' : 'apagado', p.eco),
    5: simple('Redoble'), 6: simple('Subida y caída'),
    7: simple('Más lento'), 9: simple('Más rápido', `${p.tempo} %`),
    8: simple('Ritmo', RITMOS[p.ritmo] ?? p.ritmo, p.ritmo !== 'apagado'),
    0: simple('Acordes', p.acordes ? 'encendidos' : 'apagados', p.acordes),
    '*': modo === 'cancion' ? { texto: 'Grabar loop', estado: 'en Libre', encendida: false, sirve: false }
      : simple(p.capas ? 'Encimar capa' : 'Grabar loop', p.loop === 'grabando' ? 'grabando' : null, p.loop === 'grabando'),
    '#': simple('Loop', LOOPS[p.loop] ?? p.loop, p.loop === 'sonando'),
    D: simple('Deshacer capa', p.capas === 1 ? '1 capa' : `${p.capas} capas`),
  }[tecla];
}

// alResponder(ms): cuánto tardó el osito en confirmar una tecla tocada aquí.
export function crearPanel(caja, alResponder = () => {}) {
  let datos = null;
  let modo = 'cancion';
  let enLinea = false;
  const esperando = []; // { tecla, desde, reloj } de lo tocado aquí y aún sin confirmar
  const ayuda = el('p', { class: 'texto-ayuda' });
  const error = el('p', { class: 'panel-dj__error', role: 'alert', hidden: true });
  const botones = Object.fromEntries(TECLAS.map((tecla) => {
    const texto = el('span', { class: 'tecla__texto' });
    const estado = el('span', { class: 'tecla__estado' });
    const animal = ANIMAL[tecla]?.[0];
    const boton = el('button', {
      type: 'button', class: `tecla${animal ? ' tecla--animal' : ''}`, 'data-tecla': tecla,
      style: animal ? `--animal-claro: var(--${animal}-claro); --animal-fuerte: var(--${animal}-fuerte)` : null,
    }, el('span', { class: 'tecla__simbolo', 'aria-hidden': 'true' }, animal ? cara(animal, 26) : null, animal ? el('small', {}, tecla) : tecla), texto, estado);
    prepararBoton(tecla, boton);
    return [tecla, { boton, texto, estado }];
  }));
  const borrar = el('button', { type: 'button', class: 'boton', onclick: () => mandar('d') }, 'Borrar todo el loop');
  caja.replaceChildren(el('div', { class: 'panel-dj' }, TECLAS.map((t) => botones[t].boton)),
    el('div', { class: 'panel-dj__pie' }, ayuda, borrar), error);

  // D mantenida borra todo el loop, como en el teclado real.
  function prepararBoton(tecla, boton) {
    if (tecla !== 'D') {
      boton.addEventListener('click', () => mandar(tecla));
      return;
    }
    let reloj = null;
    let fueLarga = false;
    boton.addEventListener('pointerdown', () => {
      fueLarga = false;
      reloj = setTimeout(() => { fueLarga = true; mandar('d'); }, LARGA_MS);
    });
    for (const fin of ['pointerup', 'pointerleave', 'pointercancel']) boton.addEventListener(fin, () => clearTimeout(reloj));
    boton.addEventListener('click', () => {
      if (!fueLarga) mandar('D');
      fueLarga = false;
    });
  }

  function destello(boton) {
    boton.classList.add('tecla--apretada');
    setTimeout(() => boton.classList.remove('tecla--apretada'), DESTELLO_MS);
  }

  function dejarDeEsperar(uno) {
    clearTimeout(uno.reloj);
    const i = esperando.indexOf(uno);
    if (i >= 0) esperando.splice(i, 1);
    pintar();
  }

  function fallo(texto) {
    error.textContent = texto;
    error.hidden = false;
  }

  async function mandar(tecla) {
    // Se ve enseguida que se tocó; el estado solo cambia cuando el osito lo confirma.
    destello(botones[tecla === 'd' ? 'D' : tecla].boton);
    const uno = { tecla, desde: Date.now() };
    uno.reloj = setTimeout(() => { dejarDeEsperar(uno); fallo('El osito no respondió a la tecla.'); }, ESPERA_MS);
    esperando.push(uno);
    pintar();
    try {
      await tocarTecla(tecla);
      error.hidden = true;
    } catch (e) {
      console.error('[panel] No se pudo mandar la tecla:', e);
      dejarDeEsperar(uno);
      fallo(explicar(e, 'No se pudo tocar la tecla'));
    }
  }

  function pintar() {
    const listo = enLinea && datos !== null;
    for (const tecla of TECLAS) {
      const { boton, texto, estado } = botones[tecla];
      const d = datos ? describir(tecla, datos, modo) : { texto: ANIMAL[tecla]?.[1] ?? MODO[tecla]?.[1] ?? '', estado: null, encendida: false, sirve: false };
      texto.textContent = d.texto;
      const enviando = esperando.some((e) => (e.tecla === 'd' ? 'D' : e.tecla) === tecla);
      const callado = Boolean(ANIMAL[tecla]) && d.estado === 'en silencio';
      const palabra = enviando ? 'enviando…' : d.estado ?? '';
      const clave = `${palabra}|${callado}`;
      if (estado.dataset.clave !== clave) {
        estado.dataset.clave = clave;
        const bocina = ANIMAL[tecla] && d.estado && !enviando ? icono(callado ? 'bocinaTachada' : 'bocina', 15, 2.4) : null;
        estado.replaceChildren(...(bocina ? [bocina] : []), palabra);
      }
      boton.classList.toggle('tecla--enviando', enviando);
      boton.classList.toggle('tecla--callada', listo && callado);
      boton.disabled = !listo || !d.sirve;
      boton.classList.toggle('tecla--encendida', listo && d.encendida);
      boton.setAttribute('aria-label', `Tecla ${tecla}: ${d.texto}${palabra ? `, ${palabra}` : ''}`);
    }
    borrar.hidden = !listo || modo === 'simon' || !datos.capas;
    if (!enLinea) ayuda.textContent = 'El osito no está en línea.';
    else if (!datos) ayuda.textContent = 'Esperando al osito…';
    else if (modo === 'simon') ayuda.textContent = `Simón Dice, nivel ${datos.nivel}`;
    else ayuda.textContent = `Velocidad ${datos.tempo} % · Loop ${LOOPS[datos.loop] ?? datos.loop}`;
  }

  pintar();
  return {
    actualizar(nuevo) { datos = nuevo; pintar(); },
    conexion(si) { if (si !== enLinea) { enLinea = si; pintar(); } },
    modo(nuevo) { if (nuevo !== modo) { modo = nuevo; pintar(); } },
    destellar(tecla) {
      const boton = botones[tecla === 'd' ? 'D' : tecla]?.boton;
      if (!boton) return;
      const mia = esperando.find((e) => e.tecla === tecla);
      if (mia) {
        alResponder(Date.now() - mia.desde);
        dejarDeEsperar(mia); // su destello ya se vio al tocarla
        return;
      }
      destello(boton);
    },
  };
}
