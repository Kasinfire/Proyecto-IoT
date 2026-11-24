// Caritas pixeladas de los animalitos y los datos fijos de cada uno.
// Las usan todas las pantallas, así cada animal se ve igual en todos lados.

const SVG = 'http://www.w3.org/2000/svg';

const NEGRO = '#1c1c1a';
const ROSA = '#f08fae';

// Cada fila es una línea de píxeles; cada letra, un color. "." es transparente.
const DIBUJOS = {
  osito: {
    filas: ['.BB......BB.', 'BbbB....BbbB', 'BBBBBBBBBBBB', 'BBBBBBBBBBBB', 'BBkkBBBBkkBB', 'BBkkBBBBkkBB',
      'BpBBmmmmBBpB', 'BBBmmkkmmBBB', 'BBBmmmmmmBBB', 'BBBBmkkmBBBB', 'BBBBBBBBBBBB', '.BBBBBBBBBB.'],
    colores: { B: '#9a6334', b: '#d9a06a', k: NEGRO, m: '#ecc79e', p: ROSA },
  },
  ranita: {
    filas: ['.www....www.', '.wkw....wkw.', 'GwwwGGGGwwwG', 'GGGGGGGGGGGG', 'GGGGGGGGGGGG', 'GpGGGGGGGGpG',
      'GGkGGGGGGkGG', 'GGGkkkkkkGGG', 'GggggggggggG', 'GggggggggggG', 'GGggggggggGG', '.GGGGGGGGGG.'],
    colores: { G: '#4fae4f', g: '#a6dc74', w: '#ffffff', k: NEGRO, p: ROSA },
  },
  gatito: {
    filas: ['C..........C', 'CC........CC', 'CpC......CpC', 'CCCCCCCCCCCC', 'CCoCCooCCoCC', 'CCCCCCCCCCCC',
      'CCekCCCCekCC', 'CCkkCCCCkkCC', 'kCCCCppCCCCk', 'CCCCkCCkCCCC', 'CCCCCkkCCCCC', '.CCCCCCCCCC.'],
    colores: { C: '#f3e6cf', o: '#e39b4b', e: '#4fae4f', k: NEGRO, p: ROSA },
  },
  pollito: {
    filas: ['.....YY.....', '....YY......', '..YYYYYYYY..', '.YYYYYYYYYY.', 'YYYYYYYYYYYY', 'YYkkYYYYkkYY',
      'YYkkYYYYkkYY', 'YpYYYOOYYYpY', 'YYYYOOOOYYYY', 'YYYYYOOYYYYY', '.YYyyyyyyYY.', '..YYYYYYYY..'],
    colores: { Y: '#f7c531', y: '#ffe58a', O: '#ee7d22', k: NEGRO, p: ROSA },
  },
};

// Datos fijos de cada animal. El orden es el de la orquesta en pantalla.
export const ANIMALES = [
  { id: 'osito', nombre: 'Osito', parte: 'Base (bajo) · director', instrumento: 'bajo' },
  { id: 'ranita', nombre: 'Ranita', parte: 'Acompañamiento (violín)', instrumento: 'violín' },
  { id: 'gatito', nombre: 'Gatito', parte: 'Melodía (piano)', instrumento: 'piano' },
  { id: 'pollito', nombre: 'Pollito', parte: 'Percusión (maraca)', instrumento: 'maraca' },
];

export function datosAnimal(id) {
  return ANIMALES.find((a) => a.id === id);
}

// Crea un <svg> pixelado a partir de filas de letras.
export function dibujoPixeles(filas, colores, ancho, alto = ancho) {
  const svg = document.createElementNS(SVG, 'svg');
  svg.setAttribute('viewBox', `0 0 ${filas[0].length} ${filas.length}`);
  svg.setAttribute('width', ancho);
  svg.setAttribute('height', alto);
  svg.setAttribute('shape-rendering', 'crispEdges');
  svg.setAttribute('aria-hidden', 'true');
  svg.classList.add('cara');
  filas.forEach((fila, y) => {
    [...fila].forEach((letra, x) => {
      const color = colores[letra];
      if (!color) return;
      const px = document.createElementNS(SVG, 'rect');
      px.setAttribute('x', x);
      px.setAttribute('y', y);
      px.setAttribute('width', 1);
      px.setAttribute('height', 1);
      px.setAttribute('fill', color);
      svg.append(px);
    });
  });
  return svg;
}

export function cara(id, tamano) {
  const dibujo = DIBUJOS[id];
  return dibujoPixeles(dibujo.filas, dibujo.colores, tamano);
}

// Carita dentro de su cuadro de color (el tono claro del animal).
export function cuadroCara(id, tamano, tamanoCara, fondo = `var(--${id}-claro)`) {
  const cuadro = document.createElement('div');
  cuadro.className = 'cuadro-cara';
  cuadro.style.width = `${tamano}px`;
  cuadro.style.height = `${tamano}px`;
  cuadro.style.background = fondo;
  cuadro.append(cara(id, tamanoCara));
  return cuadro;
}

// Pone el logo (carita del osito) en cada elemento con data-logo.
export function ponerLogos() {
  document.querySelectorAll('[data-logo]').forEach((el) => el.replaceChildren(cara('osito', 28)));
}
