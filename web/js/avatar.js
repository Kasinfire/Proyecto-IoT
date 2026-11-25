// Avatar de un niño: un animalito de pixel art con ropa y accesorios. Solo se guardan los ids de
// cada parte; Node-RED acepta los mismos (nodered/avatar.js) y una prueba revisa que coincidan.

import { DIBUJOS } from './caritas.js';

const SVG = 'http://www.w3.org/2000/svg';
const LADO = 22;
const CARA_X = 5;
const CARA_Y = 5;
const CUERPO_X = 4;
const CUERPO_Y = 17;
const NEGRO = '#1c1c1a';

// Cada color: [tono, sombra]
const COLORES = {
  rojo: ['#e0473f', '#a82a2a'], naranja: ['#f08a35', '#b85c14'], amarillo: ['#f7c531', '#b88a0c'],
  verde: ['#43a35a', '#24703a'], azul: ['#3f7fd6', '#2452a0'], morado: ['#8b5fd0', '#5d3a9c'],
  rosa: ['#ee7fae', '#b94a7c'], negro: ['#4a4a52', '#26262c'],
};
const COLOR_ANIMAL = {
  osito: ['#9a6334', '#ecc79e'], ranita: ['#4fae4f', '#a6dc74'], gatito: ['#f3e6cf', '#ffffff'], pollito: ['#f7c531', '#ffe58a'],
};
// Ojos de cada animal respecto a su cara, para acomodar lentes y gorros.
const OJOS_Y = { osito: 4, ranita: 0, gatito: 6, pollito: 5 };
const FONDOS = {
  durazno: 'var(--osito-claro)', menta: 'var(--ranita-claro)', lavanda: 'var(--gatito-claro)', limon: 'var(--pollito-claro)',
  rosa: 'var(--rosa)', lima: 'var(--lima-claro)',
};

// c = color elegido, d = su sombra. Los gorros van alineados con la cara (12 de ancho).
const GORROS = {
  gorra: { y: -3, filas: ['............', '...cccccc...', '..cccccccc..', '..cccccccc..', '..dddddddddd'] },
  lana: { y: -4, filas: ['.....ww.....', '.....ww.....', '...cccccc...', '..cccccccc..', '..cdcdcdcd..', '..wwwwwwww..'] },
  corona: { y: -3, filas: ['............', '..y..yy..y..', '..yy.yy.yy..', '..yyyyyyyy..', '..ycyccycy..'] },
  mago: { y: -4, filas: ['......c.....', '.....cc.....', '....cycc....', '....cccc....', '...ccccyc...', '.dddddddddd.'] },
  mono: { y: -3, filas: ['............', '..ccc..ccc..', '..cccddccc..', '..cccddccc..', '..cc....cc..'] },
  flor: { y: -3, filas: ['.......cc...', '......cccc..', '.....ccyycc.', '.....ccyycc.', '......cccc..', '.......cc...'] },
  audifonos: { x: -1, y: -3, fijo: true, filas: ['....cccccc....', '..ccc....ccc..', '.cc........cc.', '.c..........c.', 'c............c', 'c............c', 'd............d', 'dd..........dd', 'dd..........dd', 'dd..........dd'] },
};
// Centrados en los ojos; la ranita los tiene más grandes y más arriba.
const LENTES = {
  redondos: { dy: -1, filas: ['.bbbb..bbbb.', '.b..bbbb..b.', '.b..b..b..b.', '.bbbb..bbbb.'] },
  sol: { dy: -1, filas: ['kkkkkkkkkkkk', '.kwkk..kwkk.', '.kkkk..kkkk.', '..kk....kk..'] },
  corazones: { dy: -1, filas: ['rr.rr..rr.rr', 'rrrrrrrrrrrr', '.rrr....rrr.', '..r......r..'] },
};
const LENTES_RANITA = {
  redondos: { dy: -1, filas: ['bbbbb..bbbbb', 'b...b..b...b', 'b...bbbb...b', 'b...b..b...b', 'bbbbb..bbbbb'] },
  sol: { dy: -1, filas: ['kkkkkkkkkkkk', 'kwwkk..kwwkk', 'kkkkk..kkkkk', '.kkk....kkk.'] },
  corazones: { dy: -1, filas: ['rr.rr..rr.rr', 'rrrrr..rrrrr', 'rrrrrrrrrrrr', '.rrr....rrr.', '..r......r..'] },
};
const CUERPO = ['..MMMMMMMMMM..', '.MMMMLLLLMMMM.', 'MMMMLLLLLLMMMM', 'MMMMLLLLLLMMMM', 'MMMMLLLLLLMMMM'];
const ROPAS = {
  camiseta: ['..ccc....ccc..', '.cccccccccccc.', 'cccccccccccccc', 'dd.cccccccc.dd', '...cccccccc...'],
  rayas: ['..ccc....ccc..', '.wwwwwwwwwwww.', 'cccccccccccccc', 'ww.wwwwwwww.ww', '...cccccccc...'],
  overol: ['...cc....cc...', '...cc....cc...', '...cyccccyc...', '...ccddddcc...', '..cccddddccc..'],
  vestido: ['...cc....cc...', '...cccccccc...', '...cdccccdc...', '..cccccccccc..', '.ccwccwwccwcc.'],
  sudadera: ['..cddddddddc..', '.cccwccccwccc.', 'ccccwccccwcccc', 'cccccccccccccc', 'ccc.dddddd.ccc'],
  capa: ['..cccccccccc..', '.ccc..yy..ccc.', 'ccc........ccc', 'cc..........cc', 'cc..........cc'],
};
const ACCESORIOS = {
  bufanda: { x: 4, y: 16, filas: ['..rrwwrrwwrr..', '..rrwwrrwwrr..', '.........rr...', '.........ww...', '.........rr...'] },
  corbatin: { x: 4, y: 16, filas: ['....rr..rr....', '....rrRRrr....', '....rr..rr....'] },
  collar: { x: 4, y: 17, filas: ['...w......w...', '....w....w....', '.....w.ww.....', '......ww......'] },
  medalla: { x: 4, y: 17, filas: ['....b....b....', '.....b..b.....', '......yy......', '.....yyyy.....', '......yy......'] },
  globo: { x: 17, y: 3, filas: ['.rrr.', 'rwrrr', 'rwrrr', 'rrrrr', '.rrr.', '..r..', '..k..', '..k..', '.k...', '.k...', '..k..', '..k..', '.k...', '.k...', 'k....', 'k....'] },
};
const FIJOS = { k: NEGRO, w: '#ffffff', y: '#ffd84a', r: '#e0473f', R: '#a82a2a', b: '#3f7fd6' };

const COLORES_OPCIONES = [['rojo', 'Rojo'], ['naranja', 'Naranja'], ['amarillo', 'Amarillo'], ['verde', 'Verde'], ['azul', 'Azul'], ['morado', 'Morado'], ['rosa', 'Rosa'], ['negro', 'Negro']];

// Lo que se puede elegir, en el orden del editor: [id, palabra]
export const OPCIONES = {
  animal: { titulo: 'Animal', opciones: [['osito', 'Osito'], ['ranita', 'Ranita'], ['gatito', 'Gatito'], ['pollito', 'Pollito']] },
  gorro: { titulo: 'En la cabeza', opciones: [['ninguno', 'Nada'], ['gorra', 'Gorra'], ['lana', 'De lana'], ['corona', 'Corona'], ['mago', 'De mago'], ['mono', 'Moño'], ['flor', 'Flor'], ['audifonos', 'Audífonos']] },
  color_gorro: { titulo: 'Color de la cabeza', opciones: COLORES_OPCIONES },
  lentes: { titulo: 'Lentes', opciones: [['ninguno', 'Nada'], ['redondos', 'Redondos'], ['sol', 'De sol'], ['corazones', 'Corazones']] },
  ropa: { titulo: 'Ropa', opciones: [['ninguna', 'Nada'], ['camiseta', 'Camiseta'], ['rayas', 'De rayas'], ['overol', 'Overol'], ['vestido', 'Vestido'], ['sudadera', 'Sudadera'], ['capa', 'Capa']] },
  color_ropa: { titulo: 'Color de la ropa', opciones: COLORES_OPCIONES },
  accesorio: { titulo: 'Accesorio', opciones: [['ninguno', 'Nada'], ['bufanda', 'Bufanda'], ['corbatin', 'Corbatín'], ['collar', 'Collar'], ['medalla', 'Medalla'], ['globo', 'Globo']] },
  fondo: { titulo: 'Fondo', opciones: [['durazno', 'Durazno'], ['menta', 'Menta'], ['lavanda', 'Lavanda'], ['limon', 'Limón'], ['rosa', 'Rosa'], ['lima', 'Lima']] },
};

// Color de muestra de las opciones que son un color.
export function muestra(parte, id) {
  if (parte === 'fondo') return FONDOS[id];
  return parte.startsWith('color_') ? COLORES[id][0] : null;
}

// Siempre el mismo para el mismo alias, mientras no le hagan uno.
export function avatarPorDefecto(alias) {
  const suma = [...String(alias ?? '')].reduce((t, letra) => (t * 31 + letra.codePointAt(0)) % 9973, 7);
  const de = (parte, n) => { const o = OPCIONES[parte].opciones; return o[Math.floor(suma / n) % o.length][0]; };
  return { animal: de('animal', 1), gorro: 'ninguno', lentes: 'ninguno', ropa: 'camiseta', accesorio: 'ninguno', color_gorro: 'azul', color_ropa: de('color_ropa', 4), fondo: de('fondo', 32) };
}

export function avatarAlAzar() {
  return Object.fromEntries(Object.entries(OPCIONES).map(([parte, { opciones }]) => [parte, opciones[Math.floor(Math.random() * opciones.length)][0]]));
}

export function describirAvatar(a) {
  const palabra = (parte) => OPCIONES[parte].opciones.find(([id]) => id === a[parte])?.[1] ?? '';
  const antes = { lana: 'gorro ', mago: 'gorro ', redondos: 'lentes ', sol: 'lentes ', corazones: 'lentes de ', rayas: 'playera ' };
  const puesto = ['gorro', 'lentes', 'ropa', 'accesorio'].filter((p) => !a[p].startsWith('ningun')).map((p) => (antes[a[p]] ?? '') + palabra(p).toLowerCase());
  return palabra('animal') + (puesto.length ? ` con ${puesto.join(', ')}` : '');
}

function dibujo(a) {
  const lienzo = Array.from({ length: LADO }, () => Array(LADO).fill(null));
  const poner = (filas, x0, y0, colores) => filas.forEach((fila, y) => [...fila].forEach((letra, x) => {
    const color = colores[letra];
    if (color && y0 + y >= 0 && y0 + y < LADO && x0 + x >= 0 && x0 + x < LADO) lienzo[y0 + y][x0 + x] = color;
  }));
  const cara = DIBUJOS[a.animal];
  const [M, L] = COLOR_ANIMAL[a.animal];
  poner(CUERPO, CUERPO_X, CUERPO_Y, { M, L });
  if (ROPAS[a.ropa]) poner(ROPAS[a.ropa], CUERPO_X, CUERPO_Y, { ...FIJOS, c: COLORES[a.color_ropa][0], d: COLORES[a.color_ropa][1] });
  // Las pupilas de la ranita aquí no se mueven.
  const esRanita = a.animal === 'ranita';
  poner(cara.filas, CARA_X, CARA_Y, esRanita ? { ...cara.colores, A: NEGRO, B: NEGRO } : cara.colores);
  const lentes = (esRanita ? LENTES_RANITA : LENTES)[a.lentes];
  if (lentes) poner(lentes.filas, CARA_X, CARA_Y + OJOS_Y[a.animal] + lentes.dy, FIJOS);
  const gorro = GORROS[a.gorro];
  if (gorro) {
    const sube = esRanita && !gorro.fijo ? Math.max(gorro.y + CARA_Y - 2, 0) - CARA_Y : gorro.y;
    poner(gorro.filas, CARA_X + (gorro.x ?? 0), CARA_Y + sube, { ...FIJOS, c: COLORES[a.color_gorro][0], d: COLORES[a.color_gorro][1] });
  }
  const acc = ACCESORIOS[a.accesorio];
  if (acc) poner(acc.filas, acc.x, acc.y, FIJOS);
  return lienzo;
}

// <svg> del avatar. Los píxeles iguales seguidos van en un solo rectángulo.
export function dibujarAvatar(a, tamano) {
  const svg = document.createElementNS(SVG, 'svg');
  const atributos = { viewBox: `0 0 ${LADO} ${LADO}`, width: tamano, height: tamano, 'shape-rendering': 'crispEdges', 'aria-hidden': 'true', class: 'cara' };
  for (const [n, v] of Object.entries(atributos)) svg.setAttribute(n, v);
  dibujo(a).forEach((fila, y) => {
    for (let x = 0; x < LADO;) {
      const color = fila[x];
      let fin = x + 1;
      while (fin < LADO && fila[fin] === color) fin += 1;
      if (color) {
        const r = document.createElementNS(SVG, 'rect');
        for (const [n, v] of Object.entries({ x, y, width: fin - x, height: 1, fill: color })) r.setAttribute(n, v);
        svg.append(r);
      }
      x = fin;
    }
  });
  return svg;
}

// El avatar en su cuadro de color. Decorativo: el nombre siempre va al lado.
export function cuadroAvatar(avatar, alias, tamano) {
  const a = avatar ?? avatarPorDefecto(alias);
  const cuadro = document.createElement('div');
  cuadro.className = 'cuadro-cara avatar';
  Object.assign(cuadro.style, { width: `${tamano}px`, height: `${tamano}px`, background: FONDOS[a.fondo] });
  cuadro.append(dibujarAvatar(a, tamano));
  return cuadro;
}
