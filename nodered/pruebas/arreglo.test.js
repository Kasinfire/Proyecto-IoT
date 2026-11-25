'use strict';
const test = require('node:test');
const assert = require('node:assert');
const { execFileSync } = require('node:child_process');
const path = require('node:path');
const A = require('../arreglo.js');

test('cada canción incluida cuadra en compases, rango y acordes', () => {
  const crudas = require('../canciones.json');
  const ids = new Set();
  for (const [i, c] of A.incluidas().entries()) {
    assert.strictEqual(A.revisar(c), null, c.id);
    assert.match(c.id, /^[a-z0-9-]{1,23}$/);
    ids.add(c.id);
    const compases = crudas[i].melodia.split('|').map((x) => A.pulsosDe(A.leerMelodia(x)));
    compases.forEach((p, n) => assert.strictEqual(p, c.compas, `${c.id} compás ${n + 1}`));
    assert.strictEqual(c.acordes.length, compases.length, `${c.id}: acordes`);
    for (const a of c.acordes.flat()) assert.ok(A.ACORDES[a], `${c.id} ${a}`);
  }
  assert.strictEqual(ids.size, crudas.length);
});

test('las partes duran lo mismo y caben en el osito', () => {
  for (const c of A.incluidas()) {
    const p = A.arreglar(c);
    const total = A.pulsosDe(p.gatito);
    assert.strictEqual(A.pulsosDe(p.osito), total, c.id);
    assert.strictEqual(A.pulsosDe(p.ranita), total, c.id);
    assert.strictEqual(A.pulsosDe(p.pollito), c.compas, c.id); // se repite cada compás
    for (const parte of A.PARTES) {
      assert.ok(p[parte].length <= A.PASOS_MAX);
      for (const [n, d] of p[parte]) assert.ok(Number.isInteger(n) && n >= 0 && n <= 127 && Number.isInteger(d) && d >= 1 && d <= 32, `${c.id} ${parte} ${n},${d}`);
    }
  }
});

test('acordes automáticos y compases completos', () => {
  const p = A.arreglar({ melodia: A.leerMelodia('do:2 mi:2 sol:4 fa:2 la:2 fa:4 sol:2 si:2 re:4 do:4'), compas: 8, estilo: 'suave' });
  assert.deepStrictEqual(p.osito.map(([n]) => n % 12), [0, 5, 7, 0]);
  assert.strictEqual(A.pulsosDe(p.gatito), 32);
});

test('rechaza canciones que no se pueden tocar', () => {
  const buena = { nombre: 'x', tipo: 'mias', estilo: 'marcha', compas: 8, pulso_ms: 250, melodia: [[60, 2]] };
  assert.strictEqual(A.revisar(buena), null);
  for (const cambio of [{ nombre: ' ' }, { nombre: 'x'.repeat(41) }, { nombre: 5 }, { tipo: 'x' }, { estilo: 'x' }, { estilo: 'vals' },
    { compas: 7 }, { pulso_ms: 10 }, { pulso_ms: 250.5 }, { melodia: [] }, { melodia: 'do' }, { melodia: [[60]] }, { melodia: [[200, 1]] },
    { melodia: [[60, 0]] }, { melodia: [[60, 33]] }, { melodia: [[0, 4]] }, { melodia: Array(128).fill([60, 1]) }, { melodia: [null] }]) {
    assert.ok(A.revisar({ ...buena, ...cambio }), JSON.stringify(cambio).slice(0, 60));
  }
});

test('el header del osito está al día', () => {
  execFileSync(process.execPath, [path.join(__dirname, '../../herramientas/generar_canciones.mjs'), '--revisar']);
});
