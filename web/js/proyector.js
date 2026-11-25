// Pantalla del niño en el proyector. Casi sin texto: el animal que suena brinca y le salen notas,
// en Simón Dice una flecha señala a quién tocar, y el que está apagado aparece dormido.

import { ANIMALES, cara, dibujoPixeles } from './caritas.js';
import { conectar, esEnsayo } from './conexion.js';

const ANCHO = 1440;
const ALTO = 810;
const ACTIVO_MS = 2000;
const RESULTADO_VISIBLE_MS = 1600;
const BLOQUES_AVANCE = 12;
const CENTROS = [240, 560, 880, 1200];

const CREMA = { w: '#fff3d6', k: '#3b2412' };
const GESTOS = {
  osito: ['.....ww..', '.....w.w.', '.....w...', '..www....', '.wwww....', '.www.....', '.........'],   // batuta
  ranita: ['.w.w.w...', '.w.w.w.w.', '.wwwwwww.', 'wwwwwwww.', '.wwwwwww.', '..wwwww..', '..wwwww..'],  // mano que tapa
  gatito: ['wkwkwwkwk', 'wkwkwwkwk', 'wkwkwwkwk', 'wwwwwwwww', 'wwwwwwwww', 'wwwwwwwww', '.........'],  // teclas
  pollito: ['..w...w..', '.w.....w.', 'w..www..w', '.w.....w.', '..w...w..', '.........', '.........'], // sacudir
};
const LUNA = ['..www....', '.ww......', 'ww.......', 'ww.......', '.ww......', '..www....', '.........'];
const NOTA = ['...kk..', '...kkk.', '...k.kk', '...k..k', '...k...', '.kkk...', 'kkkk...', 'kkkk...', '.kk....'];
const FLECHA = ['..www..', '..www..', '..www..', 'wwwwwww', '.wwwww.', '..www..', '...w...'];
const ESTRELLA = ['....y....', '...yyy...', 'yyyyyyyyy', '.yyyyyyy.', '..yyyyy..', '..yy.yy..', '.yy...yy.'];
const CUERPO = ['..MMMMMMMM..', '.MMMMMMMMMM.', 'MMMLLLLLLMMM', 'MMMLLLLLLMMM', '.MMLLLLLLMM.', '.MMMMMMMMMM.', '..MM....MM..', '..DD....DD..'];
const COLORES_CUERPO = {
  osito: { M: '#9a6334', L: '#ecc79e', D: '#5e3a1c', nota: '#e8956b' },
  ranita: { M: '#4fae4f', L: '#a6dc74', D: '#2f7a33', nota: '#2f8f4a' },
  gatito: { M: '#f3e6cf', L: '#ffffff', D: '#c9b08a', nota: '#6f5fd6' },
  pollito: { M: '#f7c531', L: '#ffe58a', D: '#ee7d22', nota: '#d08a00' },
};
const NOMBRE_SIMON = { osito: 'EL OSITO', ranita: 'LA RANITA', gatito: 'EL GATITO', pollito: 'EL POLLITO' };

const escenario = document.getElementById('escenario');

const estado = {
  modo: 'cancion',
  progreso: null,
  estrellas: 0,
  secuencia: false,        // el reto tiene varios pasos: no se señala a uno solo
  objetivo: null,          // animal que pidió el osito en Simón Dice
  resultado: null,         // { texto, hasta }
  animales: Object.fromEntries(ANIMALES.map((a) => [a.id, { conectado: true, ultimoGesto: 0 }])),
};

// ---------- Escenario fijo ----------

function bloque(clase, x, y, ancho, alto) {
  const b = document.createElement('span');
  b.className = clase;
  Object.assign(b.style, { left: `${x}px`, top: `${y}px`, width: `${ancho}px`, height: `${alto}px` });
  return b;
}

function armarEscenario() {
  const fondo = [];
  for (const [x, y] of [[150, 120], [1130, 170]]) {
    fondo.push(bloque('nube', x, y + 24, 136, 32), bloque('nube', x + 32, y, 72, 24));
  }
  for (const [x, w, h] of [[0, 320, 72], [320, 280, 48], [600, 360, 88], [960, 240, 56], [1200, 240, 80]]) {
    fondo.push(bloque('cerro', x, 640 - h, w, h));
  }
  fondo.push(bloque('suelo', 0, 640, ANCHO, 170));
  const pasto = document.createElement('div');
  pasto.className = 'pasto';
  pasto.append(...Array.from({ length: 60 }, () => document.createElement('span')));
  fondo.push(pasto);
  for (const [x, y] of [[96, 712], [336, 760], [624, 704], [888, 752], [1128, 712], [1320, 760], [216, 784], [1008, 784]]) {
    fondo.push(bloque('mancha', x, y, 24, 24));
  }
  escenario.prepend(...fondo, ...ANIMALES.map(armarAnimal));
}

function armarAnimal({ id, nombre }, i) {
  const col = COLORES_CUERPO[id];
  const animal = document.createElement('div');
  animal.className = 'animal';
  animal.id = `animal-${id}`;
  animal.style.left = `${CENTROS[i] - 100}px`;

  const cuerpo = document.createElement('div');
  cuerpo.className = 'animal__cuerpo';
  cuerpo.append(cara(id, 132), dibujoPixeles(CUERPO, col, 132, 88));

  const letrero = document.createElement('div');
  letrero.className = 'animal__bloque';
  const gesto = document.createElement('span');
  gesto.className = 'animal__gesto';
  gesto.append(dibujoPixeles(GESTOS[id], CREMA, 36, 28));
  letrero.append(gesto, nombre.toUpperCase());

  const nota = dibujoPixeles(NOTA, { k: col.nota }, 42, 54);
  nota.classList.add('animal__nota');
  const flecha = dibujoPixeles(FLECHA, { w: '#1c1c1a' }, 70, 70);
  flecha.classList.add('animal__flecha');
  const zz = document.createElement('span');
  zz.className = 'animal__zz';
  zz.textContent = 'Zz';

  animal.append(flecha, nota, zz, cuerpo, letrero);
  return animal;
}

function armarLetreros() {
  document.getElementById('juntos-lugares').append(...ANIMALES.map(({ id }) => {
    const lugar = document.createElement('div');
    lugar.className = 'juntos__lugar';
    lugar.id = `lugar-${id}`;
    lugar.append(cara(id, 36));
    return lugar;
  }));
  document.getElementById('estrella-icono').append(dibujoPixeles(ESTRELLA, { y: '#ffd84a' }, 36, 28));
  document.getElementById('avance').append(...Array.from({ length: BLOQUES_AVANCE }, () => document.createElement('span')));
}

// Escala el escenario para que quepa en cualquier proyector sin deformarse.
function ajustarTamano() {
  const escala = Math.min(innerWidth / ANCHO, innerHeight / ALTO);
  escenario.style.transform = `translate(${(innerWidth - ANCHO * escala) / 2}px, ${(innerHeight - ALTO * escala) / 2}px) scale(${escala})`;
}

// ---------- Eventos ----------

function alEvento(msg) {
  const a = estado.animales[msg.nodo];
  switch (msg.tipo) {
    case 'sesion':
    case 'modo':
      if (msg.ok !== false && msg.modo) estado.modo = msg.modo;
      break;
    case 'estado':
      if (a) a.conectado = msg.conectado;
      break;
    case 'nota':
      if (a && msg.vel > 0) a.ultimoGesto = Date.now();
      break;
    case 'sombra':
      if (a && msg.nivel > 0) a.ultimoGesto = Date.now();
      break;
    case 'agitar':
      if (a) a.ultimoGesto = Date.now();
      break;
    case 'cancion':
      estado.progreso = msg.progreso;
      break;
    case 'simon':
      if (msg.resultado) {
        // Nunca se castiga: si falla, solo se invita a intentarlo otra vez.
        const acierto = msg.resultado === 'acierto';
        if (acierto) estado.estrellas += 1;
        estado.resultado = { texto: acierto ? '¡MUY BIEN!' : '¡CASI! OTRA VEZ', hasta: Date.now() + RESULTADO_VISIBLE_MS };
        estado.objetivo = null;
      } else {
        estado.objetivo = msg.nodo;
        estado.secuencia = (msg.nivel ?? 1) > 1;
        estado.resultado = null;
      }
      break;
    default:
      break;
  }
}

// ---------- Pintar ----------

function suena(id) {
  const a = estado.animales[id];
  if (!a.conectado) return false;
  if (id === 'osito') return estado.modo !== 'simon'; // la base suena siempre fuera de Simón Dice
  return Date.now() - a.ultimoGesto < ACTIVO_MS;
}

function pintar() {
  const esSimon = estado.modo === 'simon';
  const mostrandoResultado = estado.resultado && Date.now() < estado.resultado.hasta;
  let tocando = 0;

  for (const { id } of ANIMALES) {
    const animal = document.getElementById(`animal-${id}`);
    const esObjetivo = esSimon && !estado.secuencia && estado.objetivo === id;
    const sonando = suena(id);
    if (sonando) tocando += 1;
    animal.classList.toggle('animal--suena', sonando);
    animal.classList.toggle('animal--dormido', !estado.animales[id].conectado);
    animal.classList.toggle('animal--objetivo', esObjetivo && !mostrandoResultado);
    animal.classList.toggle('animal--tenue', esSimon && !estado.secuencia && Boolean(estado.objetivo) && !esObjetivo);
    // Cuando está dormido se cambia el dibujo del gesto por una luna
    const gesto = animal.querySelector('.animal__gesto');
    const quiere = estado.animales[id].conectado ? 'gesto' : 'luna';
    if (gesto.dataset.dibujo !== quiere) {
      gesto.dataset.dibujo = quiere;
      gesto.replaceChildren(dibujoPixeles(quiere === 'luna' ? LUNA : GESTOS[id], CREMA, 36, 28));
    }
    document.getElementById(`lugar-${id}`).classList.toggle('juntos__lugar--encendido', sonando);
  }

  // Arriba: en Simón Dice, a quién tocar; en los demás modos, la meta de tocar juntos.
  document.getElementById('juntos').hidden = esSimon;
  document.getElementById('juntos').classList.toggle('juntos--todos', tocando === ANIMALES.length);
  document.getElementById('juntos-texto').textContent = tocando === ANIMALES.length ? '¡TODOS!' : '¡JUNTOS!';

  const simon = document.getElementById('simon');
  const texto = mostrandoResultado ? estado.resultado.texto : estado.objetivo && estado.secuencia ? '¡MIRA Y REPITE!' : estado.objetivo ? `¡TOCA A ${NOMBRE_SIMON[estado.objetivo]}!` : null;
  simon.hidden = !esSimon || !texto;
  if (texto && document.getElementById('simon-texto').textContent !== texto) {
    document.getElementById('simon-texto').textContent = texto;
    document.getElementById('simon-cara').replaceChildren(estado.objetivo && !estado.secuencia && !mostrandoResultado ? cara(estado.objetivo, 56) : '');
  }
  document.getElementById('estrellas').hidden = !esSimon;
  document.getElementById('estrellas-cuenta').textContent = String(estado.estrellas);

  const avance = document.getElementById('avance');
  avance.hidden = estado.modo !== 'cancion' || estado.progreso === null;
  const llenos = Math.round((estado.progreso ?? 0) * BLOQUES_AVANCE);
  [...avance.children].forEach((b, i) => b.classList.toggle('lleno', i < llenos));
}

// ---------- Arranque ----------

armarEscenario();
armarLetreros();
ajustarTamano();
addEventListener('resize', ajustarTamano);
if (esEnsayo()) document.querySelector('[data-aviso-ensayo]').hidden = false;
conectar({ alEvento });
setInterval(pintar, 200);
pintar();
