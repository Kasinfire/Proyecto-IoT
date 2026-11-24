// Arma las cuatro partes de una canción a partir de su melodía: gatito la melodía, ranita los
// acordes, osito el bajo y pollito el ritmo. Lo usan Node-RED, el generador del firmware y las
// pruebas. La melodía es una lista de [nota MIDI (0 = silencio), pulsos].
'use strict';

const TIPOS = ['tranquilas', 'alegres', 'navidad', 'cumpleanos', 'bloques', 'mias'];
const ESTILOS = ['suave', 'marcha', 'vals'];
const PARTES = ['osito', 'ranita', 'gatito', 'pollito']; // orden de protocolo::Nodo
const PASOS_MAX = 128; // por parte; igual que config::CANCION_PASOS_MAX
const PULSOS_PASO_MAX = 32;
const PULSO_MS = { min: 120, max: 800 };
const NOTA = { min: 48, max: 84 };
const SEMITONO = { do: 0, re: 2, mi: 4, fa: 5, sol: 7, la: 9, si: 11 };
const DO_CENTRAL = 60;
const ACORDES = { C: [0, 4, 7], Dm: [2, 5, 9], Em: [4, 7, 11], F: [5, 9, 12], G: [7, 11, 14], Am: [9, 12, 16] };
const MARACA = 70;
const WOOD_BLOCK = 76;

// "do:2 sol,:1 do':4 fa#:2 -:2 | …" -> [[60,2],[55,1],[72,4],[66,2],[0,2]]. Las | son solo ayuda visual.
function leerMelodia(texto) {
  return texto.split(/\s+/).filter((x) => x && x !== '|').map((x) => {
    const m = /^(do|re|mi|fa|sol|la|si|-)(#?)([,']?):(\d+)$/.exec(x);
    if (!m) throw new Error(`no entiendo "${x}"`);
    if (m[1] === '-') return [0, Number(m[4])];
    return [DO_CENTRAL + SEMITONO[m[1]] + (m[2] ? 1 : 0) + (m[3] === ',' ? -12 : m[3] ? 12 : 0), Number(m[4])];
  });
}

const pulsosDe = (pasos) => pasos.reduce((t, [, d]) => t + d, 0);

// Deja la melodía en compases completos (silencio al final).
function completar(melodia, compas) {
  const sobra = pulsosDe(melodia) % compas;
  return sobra ? [...melodia, [0, compas - sobra]] : melodia;
}

// Un acorde por compás, el que más comparte con lo que suena. Empieza y termina en do si se puede.
function elegirAcordes(melodia, compas) {
  const peso = Array.from({ length: pulsosDe(melodia) / compas }, () => ({}));
  let t = 0;
  for (const [nota, dura] of melodia) {
    for (let k = 0; k < dura; k += 1, t += 1) {
      // Lo que cae al inicio del compás manda más.
      if (nota) peso[Math.floor(t / compas)][nota % 12] = (peso[Math.floor(t / compas)][nota % 12] ?? 0) + (t % compas === 0 ? 2 : 1);
    }
  }
  return peso.map((p, i) => {
    const extremo = i === 0 || i === peso.length - 1;
    let mejor = 'C';
    let puntos = -1;
    for (const [acorde, notas] of Object.entries(ACORDES)) {
      let v = notas.reduce((s, n) => s + (p[n % 12] ?? 0), 0);
      if (acorde === 'C') v += extremo ? 2 : 0.3;
      else if (acorde === 'F' || acorde === 'G') v += 0.2;
      if (v > puntos) [mejor, puntos] = [acorde, v];
    }
    return [mejor];
  });
}

function leerAcordes(texto) {
  return texto.split('|').map((c) => c.trim().split('-'));
}

// { osito, ranita, gatito, pollito }: cada una, su lista de pasos.
function arreglar({ melodia, compas, estilo, acordes }) {
  const completa = completar(melodia, compas);
  const porCompas = acordes ?? elegirAcordes(completa, compas);
  const bajo = [];
  const acomp = [];
  for (const compasAcordes of porCompas) {
    const dura = compas / compasAcordes.length;
    for (const nombre of compasAcordes) {
      const [raiz, tercera, quinta] = ACORDES[nombre];
      const grave = 36 + (raiz % 12);
      if (estilo !== 'suave' && dura === compas) bajo.push([grave, dura / 2], [grave + 7 > 47 ? grave - 5 : grave + 7, dura / 2]);
      else bajo.push([grave, dura]);
      acomp.push([DO_CENTRAL + tercera, dura / 2 >= 2 ? Math.ceil(dura / 2) : dura]);
      if (dura / 2 >= 2) acomp.push([DO_CENTRAL + quinta, Math.floor(dura / 2)]);
    }
  }
  // Los pasos del pollito llevan además la fuerza del golpe.
  const ritmo = [];
  for (let i = 0; i < compas; i += 1) {
    if (estilo === 'suave') {
      if (i % 2 === 0) ritmo.push([MARACA, 2, i === 0 ? 70 : 35]);
    } else if (i === 0) ritmo.push([MARACA, 1, 110]);
    else if (estilo === 'vals' ? i === 2 || i === 4 : i === compas / 2) ritmo.push([WOOD_BLOCK, 1, 70]);
    else ritmo.push([MARACA, 1, 45]);
  }
  return { osito: bajo, ranita: acomp, gatito: completa, pollito: ritmo };
}

// Mensaje de error para la pantalla, o null si la canción se puede tocar.
function revisar(c) {
  if (typeof c.nombre !== 'string' || !c.nombre.trim() || c.nombre.length > 40) return 'el nombre debe tener de 1 a 40 letras';
  if (!TIPOS.includes(c.tipo)) return 'tipo desconocido';
  if (!ESTILOS.includes(c.estilo)) return 'ritmo desconocido';
  if (![6, 8].includes(c.compas)) return 'el compás debe ser de 6 u 8 pulsos';
  if (c.estilo === 'vals' && c.compas !== 6) return 'el vals necesita compás de 3/4';
  if (c.estilo === 'marcha' && c.compas !== 8) return 'la marcha necesita compás de 4/4';
  if (!Number.isInteger(c.pulso_ms) || c.pulso_ms < PULSO_MS.min || c.pulso_ms > PULSO_MS.max) return 'velocidad fuera de rango';
  if (!Array.isArray(c.melodia) || !c.melodia.length) return 'la melodía está vacía';
  if (c.melodia.length > PASOS_MAX - 1) return `la melodía es muy larga (máximo ${PASOS_MAX - 1} notas)`;
  for (const paso of c.melodia) {
    if (!Array.isArray(paso) || paso.length !== 2) return 'hay una nota mal formada';
    const [nota, dura] = paso;
    if (!Number.isInteger(nota) || (nota !== 0 && (nota < NOTA.min || nota > NOTA.max))) return 'hay una nota fuera de rango';
    if (!Number.isInteger(dura) || dura < 1 || dura > PULSOS_PASO_MAX) return 'hay una duración fuera de rango';
  }
  if (c.melodia.every(([n]) => n === 0)) return 'la melodía solo tiene silencios';
  const partes = arreglar(c);
  if (PARTES.some((p) => partes[p].length > PASOS_MAX)) return 'la canción es muy larga';
  return null;
}

function duracion({ melodia, compas, pulso_ms: pulso }) {
  const s = Math.max(1, Math.round((pulsosDe(completar(melodia, compas)) * pulso) / 1000));
  return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, '0')}`;
}

// Las incluidas, ya leídas.
function incluidas() {
  return require('./canciones.json').map((c) => ({ ...c, melodia: leerMelodia(c.melodia), acordes: leerAcordes(c.acordes), incluida: true }));
}

module.exports = { ACORDES, ESTILOS, PARTES, PASOS_MAX, TIPOS, arreglar, duracion, incluidas, leerMelodia, pulsosDe, revisar };
