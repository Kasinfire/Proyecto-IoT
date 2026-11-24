// SIMULACIÓN: avatares y caída del broker, sin placas ni broker.
'use strict';
const test = require('node:test');
const assert = require('node:assert');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { PARTES, leerAvatar } = require('../avatar.js');
const { Orquesta } = require('../orquesta.js');

const nueva = () => {
  const reloj = { t: 1000000 };
  return { reloj, o: new Orquesta({ claveHex: '0'.repeat(32), ahora: () => reloj.t }) };
};
const AVATAR = Object.fromEntries(Object.entries(PARTES).map(([p, ids]) => [p, ids.at(-1)]));

test('la página ofrece exactamente lo que Node-RED acepta', async () => {
  const web = await import(pathToFileURL(path.join(__dirname, '../../web/js/avatar.js')));
  const deWeb = Object.fromEntries(Object.entries(web.OPCIONES).map(([p, { opciones }]) => [p, opciones.map(([id]) => id)]));
  assert.deepStrictEqual(deWeb, PARTES);
  for (let i = 0; i < 200; i += 1) {
    assert.ok(leerAvatar(web.avatarPorDefecto(`A-${i}`)));
    assert.ok(leerAvatar(web.avatarAlAzar()));
  }
  assert.deepStrictEqual(web.avatarPorDefecto('A-07'), web.avatarPorDefecto('A-07'));
});

test('rechaza avatares que no son de la lista y quita campos de más', () => {
  for (const malo of [null, 'x', [], {}, { ...AVATAR, gorro: '<b>' }, { ...AVATAR, animal: undefined }, { ...AVATAR, fondo: ['lima'] }]) {
    assert.strictEqual(leerAvatar(malo), null);
  }
  assert.deepStrictEqual(leerAvatar({ ...AVATAR, extra: 1 }), AVATAR);
});

test('el avatar se guarda, se conserva al editar sin él y no se acepta uno inválido', () => {
  const { o } = nueva();
  const p = (ruta, c) => o.alHttp('POST', ruta, c).codigo;
  const tiene = () => o.alHttp('GET', 'ninos').cuerpo[0].avatar;
  assert.strictEqual(p('ninos', { alias: 'A-01', avatar: { animal: 'dragon' } }), 400);
  assert.strictEqual(o.alHttp('GET', 'ninos').cuerpo.length, 0);
  assert.strictEqual(p('ninos', { alias: 'A-01', avatar: AVATAR }), 200);
  assert.deepStrictEqual(tiene(), AVATAR);
  assert.strictEqual(p('ninos/editar', { alias: 'A-01', nuevo_alias: 'A-02', nombre: 'Ana' }), 200);
  assert.deepStrictEqual(tiene(), AVATAR);
  assert.strictEqual(p('ninos/editar', { alias: 'A-02', nuevo_alias: 'A-03', nombre: '', avatar: null }), 400);
  assert.strictEqual(o.alHttp('GET', 'ninos').cuerpo[0].alias, 'A-02');
  assert.strictEqual(p('ninos/editar', { alias: 'A-02', nuevo_alias: 'A-02', nombre: '', avatar: { ...AVATAR, gorro: 'gorra' } }), 200);
  assert.strictEqual(tiene().gorro, 'gorra');
});

test('si cae el broker no se sabe nada y no se aceptan órdenes; al volver se recupera', () => {
  const { o } = nueva();
  o.alBroker(true);
  o.alMqtt('orquesta/osito/estado', '{"estado":"online"}');
  o.alMqtt('orquesta/gatito/estado', '{"estado":"online"}');
  const r = o.alBroker(false);
  assert.strictEqual(r.ws.filter((m) => m.tipo === 'estado' && m.conectado === null).length, 4);
  assert.deepStrictEqual([r.ws.at(-1).broker, r.ws.at(-1).internet], [false, false]);
  assert.deepStrictEqual(o.alBroker(false).ws, []);
  const t = o.alHttp('POST', 'tecla', { tecla: '8' });
  assert.strictEqual(t.codigo, 503);
  assert.match(t.cuerpo.error, /broker/);
  assert.strictEqual(o.alBroker(true).ws.at(-1).broker, true);
  o.alMqtt('orquesta/osito/estado', '{"estado":"online"}');
  assert.strictEqual(o.alHttp('POST', 'tecla', { tecla: '8' }).codigo, 204);
});

test('dice desde cuándo se desconectó, solo si lo vio', () => {
  const { o, reloj } = nueva();
  o.alMqtt('orquesta/osito/estado', '{"estado":"online"}');
  assert.ok(!('desde' in o.alMqtt('orquesta/ranita/estado', '{"estado":"offline"}').ws[0]));
  o.alMqtt('orquesta/ranita/estado', '{"estado":"online"}');
  o.alMqtt('orquesta/ranita/estado', '{"estado":"offline"}');
  reloj.t += 60000;
  assert.strictEqual(o.alConectar().find((m) => m.nodo === 'ranita').desde, 1000000);
});
