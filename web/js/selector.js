// Desplegable con el estilo de la interfaz, en lugar de la lista del sistema operativo.
// El <select> original se queda oculto como fuente de verdad: las pantallas siguen leyendo
// .value, cambiándolo y escuchando 'change' igual que antes.

import { el, icono } from './comun.js';

let contador = 0;

export function mejorarSelectores(raiz = document) {
  raiz.querySelectorAll('select.selector').forEach(mejorar);
}

function mejorar(select) {
  if (select.dataset.mejorado !== undefined) return;
  select.dataset.mejorado = '';
  const n = ++contador;

  const etiqueta = select.id ? document.querySelector(`label[for="${select.id}"]`) : null;
  if (etiqueta && !etiqueta.id) etiqueta.id = `etiqueta-desplegable-${n}`;

  const texto = el('span', { class: 'desplegable__texto' });
  const boton = el('button', {
    type: 'button',
    id: `desplegable-${n}`,
    class: 'desplegable__boton',
    'aria-haspopup': 'listbox',
    'aria-expanded': 'false',
    'aria-controls': `desplegable-${n}-lista`,
    'aria-labelledby': etiqueta ? `${etiqueta.id} desplegable-${n}` : null,
  }, texto, icono('flecha', 16, 2.6));
  const lista = el('ul', {
    id: `desplegable-${n}-lista`,
    class: 'desplegable__lista',
    role: 'listbox',
    tabindex: '-1',
    hidden: true,
    'aria-labelledby': etiqueta?.id,
  });
  const caja = el('div', { class: 'desplegable' }, boton, lista);

  select.after(caja);
  select.hidden = true;
  select.tabIndex = -1;
  etiqueta?.addEventListener('click', (e) => {
    e.preventDefault();
    boton.focus();
  });

  let activa = -1;
  let busqueda = '';
  let relojBusqueda = 0;

  function pintarBoton() {
    const elegida = select.selectedOptions[0];
    texto.textContent = elegida ? elegida.textContent : 'Elegir…';
    boton.disabled = select.disabled || select.options.length === 0;
  }

  function pintarLista() {
    lista.replaceChildren(...[...select.options].map((opcion, i) => el('li', {
      id: `desplegable-${n}-op-${i}`,
      class: 'desplegable__opcion',
      role: 'option',
      'aria-selected': String(opcion.selected),
      'aria-disabled': opcion.disabled ? 'true' : null,
      onclick: () => elegir(i),
      onpointermove: () => { if (activa !== i) marcar(i, false); },
    }, el('span', {}, opcion.textContent), opcion.selected ? icono('palomita', 16, 2.6) : null)));
  }

  function marcar(i, desplazar = true) {
    const total = select.options.length;
    if (!total) return;
    activa = Math.max(0, Math.min(total - 1, i));
    [...lista.children].forEach((li, j) => li.classList.toggle('es-activa', j === activa));
    const li = lista.children[activa];
    lista.setAttribute('aria-activedescendant', li.id);
    if (desplazar) li.scrollIntoView({ block: 'nearest' });
  }

  function abrir() {
    if (boton.disabled || !lista.hidden) return;
    pintarLista();
    lista.hidden = false;
    boton.setAttribute('aria-expanded', 'true');
    caja.classList.add('esta-abierto');
    // Si no cabe abajo y arriba hay más espacio, se abre hacia arriba
    const r = boton.getBoundingClientRect();
    const abajo = window.innerHeight - r.bottom;
    caja.classList.toggle('hacia-arriba', abajo < Math.min(lista.scrollHeight, 320) + 16 && r.top > abajo);
    marcar(Math.max(0, select.selectedIndex));
    lista.focus({ preventScroll: true });
  }

  function cerrar(regresarFoco = true) {
    if (lista.hidden) return;
    lista.hidden = true;
    boton.setAttribute('aria-expanded', 'false');
    caja.classList.remove('esta-abierto');
    if (regresarFoco) boton.focus();
  }

  function elegir(i) {
    const opcion = select.options[i];
    if (!opcion || opcion.disabled) return;
    const cambio = select.selectedIndex !== i;
    select.selectedIndex = i;
    cerrar();
    if (cambio) select.dispatchEvent(new Event('change', { bubbles: true }));
  }

  // Busca por la primera letra o palabra que se teclee, como el select del navegador
  function buscar(letra) {
    clearTimeout(relojBusqueda);
    busqueda += letra.toLowerCase();
    relojBusqueda = setTimeout(() => { busqueda = ''; }, 600);
    const opciones = [...select.options];
    const desde = busqueda.length === 1 ? activa + 1 : activa;
    for (let k = 0; k < opciones.length; k++) {
      const i = (desde + k) % opciones.length;
      if (opciones[i].textContent.trim().toLowerCase().startsWith(busqueda)) return marcar(i);
    }
  }

  boton.addEventListener('click', () => (lista.hidden ? abrir() : cerrar()));
  boton.addEventListener('keydown', (e) => {
    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      e.preventDefault();
      abrir();
    }
  });

  lista.addEventListener('keydown', (e) => {
    const total = select.options.length;
    switch (e.key) {
      case 'ArrowDown': marcar(activa + 1); break;
      case 'ArrowUp': marcar(activa - 1); break;
      case 'Home': marcar(0); break;
      case 'End': marcar(total - 1); break;
      case 'PageDown': marcar(activa + 5); break;
      case 'PageUp': marcar(activa - 5); break;
      case 'Enter':
      case ' ': elegir(activa); break;
      case 'Escape': cerrar(); break;
      case 'Tab': cerrar(false); return;
      default:
        if (e.key.length === 1 && !e.ctrlKey && !e.metaKey && !e.altKey) buscar(e.key);
        return;
    }
    e.preventDefault();
  });

  lista.addEventListener('focusout', (e) => {
    if (!caja.contains(e.relatedTarget)) cerrar(false);
  });
  document.addEventListener('pointerdown', (e) => {
    if (!caja.contains(e.target)) cerrar(false);
  });

  // Cuando la pantalla cambia el valor por código (por ejemplo, lo confirmó el osito)
  for (const propiedad of ['value', 'selectedIndex']) {
    const original = Object.getOwnPropertyDescriptor(HTMLSelectElement.prototype, propiedad);
    Object.defineProperty(select, propiedad, {
      configurable: true,
      get() { return original.get.call(this); },
      set(valor) { original.set.call(this, valor); pintarBoton(); },
    });
  }
  // Cuando llegan las opciones (las canciones o los niños vienen de Node-RED)
  new MutationObserver(() => {
    pintarBoton();
    if (!lista.hidden) pintarLista();
  }).observe(select, { childList: true, subtree: true, attributes: true, attributeFilter: ['disabled'] });

  pintarBoton();
}
