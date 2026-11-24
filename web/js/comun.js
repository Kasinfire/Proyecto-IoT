// Ayudas que usan todas las pantallas: crear elementos, íconos, formatos y el encabezado.

import { ponerLogos } from './caritas.js';
import { esEnsayo } from './conexion.js';
import { sesionActual } from './api.js';

const SVG = 'http://www.w3.org/2000/svg';

// el('div', { class: 'x' }, 'texto', otroNodo). El texto siempre entra como texto, nunca como HTML.
export function el(etiqueta, atributos = {}, ...hijos) {
  const nodo = document.createElement(etiqueta);
  for (const [nombre, valor] of Object.entries(atributos)) {
    if (valor === false || valor === null || valor === undefined) continue;
    if (nombre.startsWith('on')) nodo.addEventListener(nombre.slice(2), valor);
    else if (nombre === 'style' && typeof valor === 'object') Object.assign(nodo.style, valor);
    else nodo.setAttribute(nombre, valor === true ? '' : valor);
  }
  nodo.append(...hijos.flat().filter((h) => h !== null && h !== undefined && h !== false));
  return nodo;
}

const TRAZOS = {
  onda: '<path d="M3 12h3l3-6 4 12 3-6h5"></path>',
  linea: '<path d="M6 12h12"></path>',
  tache: '<path d="M6 6l12 12M18 6 6 18"></path>',
  reloj: '<circle cx="12" cy="12" r="9"></circle><path d="M12 7v5l3 2"></path>',
  nota: '<path d="M9 18V5l11-2v13"></path><circle cx="6" cy="18" r="3"></circle><circle cx="17" cy="16" r="3"></circle>',
  flecha: '<path d="M9 6l6 6-6 6"></path>',
  mas: '<path d="M12 5v14M5 12h14"></path>',
  wifi: '<path d="M2 9a15 15 0 0 1 20 0"></path><path d="M5 12.5a10 10 0 0 1 14 0"></path><path d="M8.5 16a5 5 0 0 1 7 0"></path><path d="M12 19.5h.01"></path>',
  wifiTachado: '<path d="M2 9a15 15 0 0 1 20 0"></path><path d="M5 12.5a10 10 0 0 1 14 0"></path><path d="M8.5 16a5 5 0 0 1 7 0"></path><path d="M12 19.5h.01"></path><path d="M3 3l18 18"></path>',
  reintentar: '<path d="M21 12a9 9 0 1 1-3-6.7"></path><path d="M21 4v5h-5"></path>',
  palomita: '<path d="M5 12.5l4.5 4.5L19 7.5"></path>',
};

// Ícono de línea. Los íconos son decorativos: el texto de al lado dice lo mismo.
export function icono(nombre, tamano = 18, grosor = 2.2, color = 'currentColor') {
  const svg = document.createElementNS(SVG, 'svg');
  svg.setAttribute('width', tamano);
  svg.setAttribute('height', tamano);
  svg.setAttribute('viewBox', '0 0 24 24');
  svg.setAttribute('fill', 'none');
  svg.setAttribute('stroke', color);
  svg.setAttribute('stroke-width', grosor);
  svg.setAttribute('stroke-linecap', 'round');
  svg.setAttribute('stroke-linejoin', 'round');
  svg.setAttribute('aria-hidden', 'true');
  svg.innerHTML = TRAZOS[nombre];
  return svg;
}

// Círculo negro con palomita lima: "confirmado" / "sonando".
export function palomita(tamano = 22) {
  const marca = el('span', { class: 'palomita', style: { width: `${tamano}px`, height: `${tamano}px` } });
  marca.append(icono('palomita', Math.round(tamano / 2), 4, '#dcf16b'));
  return marca;
}

export function haceCuanto(ms) {
  const s = Math.max(0, Math.round(ms / 1000));
  if (s < 60) return `hace ${s} s`;
  return `hace ${Math.round(s / 60)} min`;
}

export function reloj(ms) {
  const s = Math.max(0, Math.floor(ms / 1000));
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}

const MESES = ['ene', 'feb', 'mar', 'abr', 'may', 'jun', 'jul', 'ago', 'sep', 'oct', 'nov', 'dic'];

// '2026-11-20' -> { dia: '20', mes: 'nov' }
export function fechaCorta(iso) {
  const [, mes, dia] = iso.split('-').map(Number);
  return { dia: String(dia), mes: MESES[mes - 1] };
}

// Cosas comunes a todas las páginas: logo, aviso de ensayo, enlaces y acceso a la sesión en curso.
export async function iniciarPagina() {
  ponerLogos();

  if (esEnsayo()) {
    document.querySelector('[data-aviso-ensayo]')?.removeAttribute('hidden');
    // Para que al navegar se siga en ensayo
    document.querySelectorAll('a[href$=".html"]').forEach((a) => {
      a.href = `${a.getAttribute('href')}?ensayo`;
    });
  }

  const enlace = document.querySelector('[data-enlace-sesion]');
  if (!enlace) return;
  try {
    const sesion = await sesionActual();
    if (sesion) {
      enlace.querySelector('[data-alias]').textContent = sesion.alias;
      enlace.removeAttribute('hidden');
    }
  } catch (error) {
    console.warn('[comun] No se pudo saber si hay sesión en curso:', error);
  }
}
