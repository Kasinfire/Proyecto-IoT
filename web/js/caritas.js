// Caritas pixeladas de los animalitos y los datos fijos de cada uno.
// Las usan todas las pantallas, así cada animal se ve igual en todos lados.

const SVG = 'http://www.w3.org/2000/svg';

const NEGRO = '#1c1c1a';
const ROSA = '#f08fae';

// Cada fila es una línea de píxeles; cada letra, un color. "." es transparente.
export const DIBUJOS = {
  osito: {
    filas: ['.BB......BB.', 'BbbB....BbbB', 'BBBBBBBBBBBB', 'BBBBBBBBBBBB', 'BBkkBBBBkkBB', 'BBkkBBBBkkBB',
      'BpBBmmmmBBpB', 'BBBmmkkmmBBB', 'BBBmmmmmmBBB', 'BBBBmkkmBBBB', 'BBBBBBBBBBBB', '.BBBBBBBBBB.'],
    colores: { B: '#9a6334', b: '#d9a06a', k: NEGRO, m: '#ecc79e', p: ROSA },
  },
  ranita: {
    filas: ['.www....www.', '.wAw....wBw.', 'GwwwGGGGwwwG', 'GGGGGGGGGGGG', 'GGGGGGGGGGGG', 'GpGGGGGGGGpG',
      'GGkGGGGGGkGG', 'GGGkkkkkkGGG', 'GggggggggggG', 'GggggggggggG', 'GGggggggggGG', '.GGGGGGGGGG.'],
    colores: { G: '#4fae4f', g: '#a6dc74', w: '#ffffff', k: NEGRO, p: ROSA },
    // Ojos que se mueven: un ojo mira tranquilo de lado a lado y el otro anda por su cuenta
    pupilas: { A: 'pupila pupila--tranquila', B: 'pupila pupila--loca' },
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
// En "pupilas", cada letra es un ojo blanco con una pupila encima que se anima con su clase CSS.
export function dibujoPixeles(filas, colores, ancho, alto = ancho, pupilas = {}) {
  const svg = document.createElementNS(SVG, 'svg');
  svg.setAttribute('viewBox', `0 0 ${filas[0].length} ${filas.length}`);
  svg.setAttribute('width', ancho);
  svg.setAttribute('height', alto);
  svg.setAttribute('shape-rendering', 'crispEdges');
  svg.setAttribute('aria-hidden', 'true');
  svg.classList.add('cara');
  const pixel = (x, y, color) => {
    const px = document.createElementNS(SVG, 'rect');
    px.setAttribute('x', x);
    px.setAttribute('y', y);
    px.setAttribute('width', 1);
    px.setAttribute('height', 1);
    px.setAttribute('fill', color);
    return px;
  };
  const encima = [];
  filas.forEach((fila, y) => {
    [...fila].forEach((letra, x) => {
      if (pupilas[letra]) {
        svg.append(pixel(x, y, '#ffffff'));
        const pupila = pixel(x, y, NEGRO);
        pupila.setAttribute('class', pupilas[letra]);
        encima.push(pupila);
        return;
      }
      const color = colores[letra];
      if (color) svg.append(pixel(x, y, color));
    });
  });
  // Las pupilas van al final para quedar encima del blanco del ojo al moverse
  svg.append(...encima);
  return svg;
}

export function cara(id, tamano) {
  const dibujo = DIBUJOS[id];
  return dibujoPixeles(dibujo.filas, dibujo.colores, tamano, tamano, dibujo.pupilas);
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
