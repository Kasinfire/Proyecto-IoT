// Convierte lo que se tocó en el gatito en una melodía para el editor: [[nota, pulsos], …].
'use strict';

// En Libre el gatito suena en pentatónica (director.cpp): se guarda lo que se oyó.
const COMO_SUENA = { do: 60, re: 62, mi: 64, fa: 67, sol: 69, la: 72, si: 74 };
const PULSO_MS = 250;
const HUECO_MAX = 8; // pulsos: una pausa larga no deja medio minuto de silencio
const PULSOS_MAX = 32;

// toques: [{ nota, inicio, fin }] en ms, en orden de inicio. fin null = no se soltó.
function aMelodia(toques, pasosMax) {
  const melodia = [];
  const conocidos = toques.filter((x) => COMO_SUENA[x.nota]);
  let t = 0; // pulso en el que va la melodía
  conocidos.forEach((x, i) => {
    const rejilla = (ms) => Math.round((ms - conocidos[0].inicio) / PULSO_MS);
    const inicio = rejilla(x.inicio);
    const siguiente = i + 1 < conocidos.length ? rejilla(conocidos[i + 1].inicio) : Infinity;
    if (siguiente === inicio) return; // dos a la vez: la melodía lleva una sola, la última
    if (inicio > t) melodia.push([0, Math.min(inicio - t, HUECO_MAX)]);
    const sostenida = Math.max(1, Math.round(((x.fin ?? x.inicio + 2 * PULSO_MS) - x.inicio) / PULSO_MS));
    const dura = Math.min(sostenida, siguiente - inicio, PULSOS_MAX);
    melodia.push([COMO_SUENA[x.nota], dura]);
    t = inicio + dura;
  });
  return melodia.slice(0, pasosMax);
}

module.exports = { PULSO_MS, aMelodia };
