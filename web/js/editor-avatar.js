// Editor del avatar: vista previa y, por cada parte, opciones con dibujo y palabra. Son radios nativos,
// así que también se usa con el teclado y con lector de pantalla.

import { OPCIONES, avatarAlAzar, describirAvatar, dibujarAvatar, cuadroAvatar, muestra } from './avatar.js';
import { el } from './comun.js';

// El color solo sirve si hay algo que pintar.
const COLOR_DE = { color_gorro: 'gorro', color_ropa: 'ropa' };
const vacia = (a, parte) => a[parte].startsWith('ningun');

export function crearEditorAvatar(caja) {
  let actual = avatarAlAzar();
  const previa = el('div', {});
  const texto = el('p', { class: 'texto-ayuda', 'aria-live': 'polite' });
  const azar = el('button', { type: 'button', class: 'boton', onclick: () => poner(avatarAlAzar()) }, 'Al azar');

  const partes = Object.entries(OPCIONES).map(([parte, { titulo, opciones }]) => {
    const radios = opciones.map(([id, palabra]) => {
      const entrada = el('input', { type: 'radio', class: 'opcion-avatar__radio', name: `avatar-${parte}`, value: id });
      entrada.addEventListener('change', () => { actual = { ...actual, [parte]: id }; pintar(); });
      const dibujo = el('span', { class: 'opcion-avatar__dibujo' });
      const color = muestra(parte, id);
      if (color) dibujo.style.background = color;
      return { id, entrada, dibujo, color, etiqueta: el('label', { class: 'opcion-avatar' }, entrada, dibujo, el('span', {}, palabra)) };
    });
    const grupo = el('fieldset', { class: 'grupo-avatar' }, el('legend', {}, titulo), el('div', { class: 'grupo-avatar__opciones' }, radios.map((r) => r.etiqueta)));
    return { parte, radios, grupo };
  });

  caja.replaceChildren(el('div', { class: 'editor-avatar__previa' }, previa, texto, azar),
    el('div', { class: 'editor-avatar__partes' }, partes.map((p) => p.grupo)));

  function pintar() {
    previa.replaceChildren(cuadroAvatar(actual, '', 132));
    texto.textContent = describirAvatar(actual);
    for (const { parte, radios, grupo } of partes) {
      if (COLOR_DE[parte]) grupo.hidden = vacia(actual, COLOR_DE[parte]);
      for (const r of radios) {
        r.entrada.checked = actual[parte] === r.id;
        if (!r.color) r.dibujo.replaceChildren(dibujarAvatar({ ...actual, [parte]: r.id }, 44));
      }
    }
  }

  function poner(avatar) {
    actual = { ...avatar };
    pintar();
  }

  return { poner, valor: () => ({ ...actual }) };
}
