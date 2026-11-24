// SIMULACIÓN: eventos cifrados como los del osito, sin placas ni broker.
'use strict';
const test = require('node:test');
const assert = require('node:assert');
const crypto = require('node:crypto');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { Orquesta } = require('../orquesta.js');

const CLAVE = '00112233445566778899aabbccddeeff';
function sobre(nodo, seq, campos, clave = CLAVE, arranque = 7) {
  const nonce = crypto.randomBytes(12);
  const c = crypto.createCipheriv('aes-128-gcm', Buffer.from(clave, 'hex'), nonce);
  c.setAAD(Buffer.from(`${nodo}:${seq}`));
  const ct = Buffer.concat([c.update(JSON.stringify({ seq, t: 0, nodo, ...campos, arranque })), c.final()]);
  return JSON.stringify({ seq, data: Buffer.concat([nonce, ct, c.getAuthTag()]).toString('base64') });
}
function nueva(extra = {}) {
  const reloj = { t: Date.now() };
  const avisos = [];
  const o = new Orquesta({ claveHex: CLAVE, ahora: () => reloj.t, aviso: (a) => avisos.push(a), ...extra });
  return { o, reloj, avisos };
}
const ack = (r) => r.mqtt.map((m) => JSON.parse(m.payload).hasta_seq);
const nota = (seq, vel = 100, arranque = 7) => ['orquesta/gatito/evento', sobre('gatito', seq, { tipo: 'nota', nota: 'mi', vel }, CLAVE, arranque)];

test('descifra, reparte y confirma', () => {
  const { o } = nueva();
  const r = o.alMqtt(...nota(1));
  assert.deepStrictEqual(r.ws, [{ tipo: 'nota', nodo: 'gatito', nota: 'mi', vel: 100 }]);
  assert.deepStrictEqual(r.mqtt, [{ topic: 'orquesta/sync/ack', payload: '{"hasta_seq":1}' }]);
});

test('rechaza alterado, clave distinta y tópico cambiado, sin confirmar', () => {
  const { o, avisos } = nueva();
  const bueno = JSON.parse(nota(1)[1]);
  const b = Buffer.from(bueno.data, 'base64');
  b[20] ^= 1;
  const casos = [
    ['orquesta/gatito/evento', JSON.stringify({ seq: 1, data: b.toString('base64') })],
    ['orquesta/gatito/evento', sobre('gatito', 1, { tipo: 'nota' }, 'f'.repeat(32))],
    ['orquesta/ranita/evento', JSON.stringify(bueno)],
    ['orquesta/gatito/evento', JSON.stringify({ ...bueno, seq: 2 })],
    ['orquesta/gatito/evento', 'basura'],
    ['orquesta/gatito/evento', '{"seq":1}'],
    ['orquesta/gatito/evento', 'null'],
  ];
  for (const c of casos) {
    const r = o.alMqtt(...c);
    assert.deepStrictEqual(r, { ws: [], mqtt: [] });
  }
  assert.strictEqual(avisos.length, casos.length);
});

test('un repetido no cuenta dos veces pero se vuelve a confirmar', () => {
  const { o } = nueva();
  o.alMqtt(...nota(1));
  const r = o.alMqtt(...nota(1));
  assert.deepStrictEqual(r.ws, []);
  assert.deepStrictEqual(ack(r), [1]);
});

test('con un hueco no confirma de más; si llega, avanza', () => {
  const { o } = nueva();
  o.alMqtt(...nota(1));
  assert.deepStrictEqual(ack(o.alMqtt(...nota(3))), [1]);
  const r = o.alMqtt(...nota(2));
  assert.strictEqual(r.ws.length, 1);
  assert.deepStrictEqual(ack(r), [3]);
});

test('si el hueco ya no existe en el osito, sigue tras la espera', () => {
  const { o, reloj, avisos } = nueva();
  o.alMqtt(...nota(1));
  o.alMqtt(...nota(5));
  reloj.t += 24000;
  assert.deepStrictEqual(o.alSegundo().mqtt, []);
  reloj.t += 2000;
  assert.deepStrictEqual(ack(o.alSegundo()), [5]);
  assert.match(avisos[0], /2 a 4/);
});

test('al reiniciarse el osito, seq 1 es nuevo', () => {
  const { o } = nueva();
  o.alMqtt(...nota(1));
  o.alMqtt(...nota(2));
  const r = o.alMqtt(...nota(1, 100, 8));
  assert.strictEqual(r.ws.length, 1);
  assert.deepStrictEqual(ack(r), [1]);
});

test('reiniciar Node-RED conserva lo confirmado, niños y sesión', () => {
  const archivo = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'orq-')), 'd', 'o.json');
  const { o } = nueva({ archivo });
  o.alHttp('POST', 'ninos', { alias: 'a-07' });
  o.alHttp('POST', 'sesion/iniciar', { alias: 'A-07' });
  o.alMqtt(...nota(1));
  const { o: p } = nueva({ archivo });
  assert.deepStrictEqual(p.alMqtt(...nota(1)).ws, []);
  assert.strictEqual(p.alConectar()[0].alias, 'A-07');
  assert.strictEqual(p.alConectar()[0].resumen.gestos.gatito, 1);
  fs.writeFileSync(archivo, '{dañado');
  assert.throws(() => nueva({ archivo })); // no escribe encima del historial
});

test('modo: responde id, espera acuse y avisa a las pantallas', () => {
  const { o } = nueva();
  assert.strictEqual(o.alHttp('POST', 'modo', { modo: 'libre' }).codigo, 503);
  o.alMqtt('orquesta/osito/estado', '{"estado":"online"}');
  const r = o.alHttp('POST', 'modo', { modo: 'libre', cancion: 'Noche de paz' });
  assert.deepStrictEqual(JSON.parse(r.mqtt[0].payload), { id: r.cuerpo.id, modo: 'libre', cancion: 'noche-de-paz' });
  const a = o.alMqtt('orquesta/control/modo/ack', JSON.stringify({ id: r.cuerpo.id, ok: true, modo: 'libre' }));
  assert.deepStrictEqual(a.ws, [{ tipo: 'modo', id: r.cuerpo.id, ok: true, modo: 'libre', cancion: 'Noche de paz' }]);
  assert.strictEqual(o.alHttp('POST', 'modo', { modo: 'x' }).codigo, 400);
  assert.strictEqual(o.alHttp('POST', 'modo', { modo: 'cancion', cancion: 'No existe' }).codigo, 400);
});

test('modo con el botón llega sin id; el de Node-RED no se repite', () => {
  const { o } = nueva();
  const e = (seq, origen) => o.alMqtt('orquesta/osito/evento', sobre('osito', seq, { tipo: 'modo', modo: 'simon', cancion: 'prueba', origen }));
  assert.deepStrictEqual(e(1, 'boton').ws, [{ tipo: 'modo', id: null, ok: true, modo: 'simon', cancion: 'prueba' }]);
  assert.deepStrictEqual(e(2, 'nodered').ws, []);
});

test('estado, volumen en % y cola', () => {
  const { o } = nueva();
  assert.deepStrictEqual(o.alMqtt('orquesta/ranita/estado', '{"estado":"offline"}').ws, [{ tipo: 'estado', nodo: 'ranita', conectado: false }]);
  assert.strictEqual(o.alMqtt('orquesta/osito/volumen', '{"valor":127}').ws[0].valor, 100);
  o.alMqtt('orquesta/osito/estado', '{"estado":"online"}');
  o.alMqtt('orquesta/osito/estado', '{"estado":"offline"}');
  assert.strictEqual(o.alConectar().find((m) => m.tipo === 'red').internet, false);
  assert.deepStrictEqual(o.alMqtt('orquesta/osito/estado', '{"estado":"online"}').ws, [{ tipo: 'estado', nodo: 'osito', conectado: true }]);
  const r = o.alMqtt('orquesta/osito/cola', '{"pendientes":12,"descartados":0}').ws[0];
  assert.strictEqual(r.internet, true);
  assert.strictEqual(r.pendientes, 12);
  assert.strictEqual(r.recuperados, 12);
  assert.deepStrictEqual(o.alMqtt('orquesta/ranita/estado', 'null'), { ws: [], mqtt: [] });
});

test('sin osito, el estado de los animales no se sabe', () => {
  const { o } = nueva();
  o.alMqtt('orquesta/gatito/estado', '{"estado":"online"}');
  const r = o.alMqtt('orquesta/osito/estado', '{"estado":"offline"}');
  assert.ok(r.ws.some((m) => m.nodo === 'gatito' && m.conectado === null));
  // El retenido puede llegar después que el del osito.
  assert.strictEqual(o.alMqtt('orquesta/gatito/estado', '{"estado":"online"}').ws[0].conectado, null);
  assert.strictEqual(o.alConectar().find((m) => m.nodo === 'gatito').conectado, null);
  o.alMqtt('orquesta/osito/estado', '{"estado":"online"}');
  assert.strictEqual(o.alMqtt('orquesta/gatito/estado', '{"estado":"online"}').ws[0].conectado, true);
});

test('niños: nombre, alias automático, editar y borrar', () => {
  const { o } = nueva();
  const p = (r, d) => o.alHttp('POST', r, d);
  assert.deepStrictEqual(p('ninos', { nombre: '  Ana   María ' }).cuerpo, { alias: 'N-01' });
  assert.deepStrictEqual(p('ninos', { nombre: 'Beto' }).cuerpo, { alias: 'N-02' });
  assert.strictEqual(p('ninos', { alias: 'n01' }).codigo, 409); // mismo que N-01
  assert.strictEqual(p('ninos', {}).codigo, 400);
  assert.strictEqual(p('ninos', { nombre: 'x'.repeat(41) }).codigo, 400);
  assert.strictEqual(o.alHttp('GET', 'ninos').cuerpo[0].nombre, 'Ana María');
  p('sesion/iniciar', { alias: 'N-01' });
  // El nombre no sale en lo que se manda a las pantallas ni se guarda en la sesión.
  assert.ok(!JSON.stringify(o.alConectar()).includes('Ana'));
  assert.strictEqual(p('ninos/borrar', { alias: 'N-01' }).codigo, 409);
  const r = p('ninos/editar', { alias: 'N-01', nuevo_alias: 'a7', nombre: 'Ana' });
  assert.deepStrictEqual(r.cuerpo, { alias: 'A-07' });
  assert.strictEqual(r.ws[0].alias, 'A-07'); // la pantalla abierta se entera
  p('sesion/terminar', {});
  assert.ok(!JSON.stringify(o.datos.sesiones).includes('Ana'));
  assert.strictEqual(p('ninos/editar', { alias: 'A-07', nuevo_alias: 'A-08', nombre: 'Ana' }).codigo, 200);
  assert.strictEqual(o.alHttp('GET', 'historial', { alias: 'A-08', periodo: 'todo' }).cuerpo.sesiones.length, 1);
  assert.strictEqual(p('ninos/editar', { alias: 'A-08', nuevo_alias: 'N-02', nombre: '' }).codigo, 409);
  assert.strictEqual(p('ninos/editar', { alias: 'A-08', nuevo_alias: 'Ana', nombre: '' }).codigo, 400);
  assert.strictEqual(p('ninos/borrar', { alias: 'A-08' }).codigo, 204);
  assert.strictEqual(o.datos.sesiones.length, 0);
  assert.strictEqual(p('ninos/borrar', { alias: 'A-08' }).codigo, 404);
  assert.deepStrictEqual(o.alHttp('GET', 'ninos').cuerpo.map((n) => n.alias), ['N-02']);
});

test('sesión completa: gestos, Simón, historial y ficha', () => {
  const { o, reloj } = nueva();
  assert.strictEqual(o.alHttp('POST', 'ninos', { alias: 'Juan Pérez' }).codigo, 400);
  assert.strictEqual(o.alHttp('POST', 'ninos', { alias: 'B-12' }).codigo, 200);
  assert.strictEqual(o.alHttp('POST', 'ninos', { alias: 'b-12' }).codigo, 409);
  assert.strictEqual(o.alHttp('POST', 'sesion/terminar', {}).codigo, 409);
  assert.strictEqual(o.alHttp('POST', 'sesion/iniciar', { alias: 'Z-99' }).codigo, 404);
  assert.strictEqual(o.alHttp('POST', 'sesion/iniciar', { alias: 'B-12' }).codigo, 204);
  assert.strictEqual(o.alHttp('POST', 'sesion/iniciar', { alias: 'B-12' }).codigo, 409);
  o.alMqtt(...nota(1));
  o.alMqtt(...nota(2, 0)); // soltar no es gesto
  for (const [s, nivel] of [[3, 40], [4, 60], [5, 0], [6, 10]]) o.alMqtt('orquesta/ranita/evento', sobre('ranita', s, { tipo: 'sombra', nivel }));
  o.alMqtt('orquesta/pollito/evento', sobre('pollito', 7, { tipo: 'agitar', intensidad: 20 }));
  o.alMqtt('orquesta/osito/estado', '{"estado":"online"}');
  o.alSegundo();
  o.alMqtt('orquesta/osito/evento', sobre('osito', 8, { tipo: 'simon', pedido: 'gatito', nivel: 1 }));
  o.alMqtt('orquesta/osito/evento', sobre('osito', 9, { tipo: 'simon', pedido: 'gatito', nivel: 1, resultado: 'acierto' }));
  o.alMqtt('orquesta/osito/evento', sobre('osito', 10, { tipo: 'simon', pedido: 'gatito', nivel: 2, resultado: 'tiempo' }));
  assert.deepStrictEqual(o.alConectar()[0].resumen, { gestos: { gatito: 1, ranita: 2, pollito: 1 }, simon: { aciertos: 1, turnos: 2 }, juntos_s: 1 });
  reloj.t += 600000;
  assert.strictEqual(o.alHttp('POST', 'sesion/terminar', { nota: 'bien' }).codigo, 204);
  const h = o.alHttp('GET', 'historial', { alias: 'B-12', periodo: 'mes' }).cuerpo;
  assert.strictEqual(h.sesiones.length, 1);
  assert.strictEqual(h.sesiones[0].gestos, 4);
  assert.strictEqual(h.sesiones[0].minutos, 10);
  assert.deepStrictEqual(h.sesiones[0].simon, [1, 2]);
  assert.deepStrictEqual(o.alHttp('GET', 'ninos').cuerpo, [{ alias: 'B-12', nombre: '', sesiones: 1, simon_pct: 50, favorito: 'ranita', en_sesion: false }]);
  reloj.t += 40 * 86400000;
  assert.strictEqual(o.alHttp('GET', 'historial', { alias: 'B-12', periodo: 'mes' }).cuerpo.sesiones.length, 0);
  assert.strictEqual(o.alHttp('GET', 'historial', { alias: 'B-12', periodo: 'todo' }).cuerpo.sesiones.length, 1);
});

const enLinea = (o) => {
  for (const n of ['osito', 'gatito']) o.alMqtt(`orquesta/${n}/estado`, '{"estado":"online"}');
};
const MIA = { nombre: 'Mi canción', tipo: 'alegres', estilo: 'marcha', compas: 8, pulso_ms: 250, melodia: Array(40).fill([60, 2]) };

test('canción propia: se manda por trozos y el modo va después del acuse', () => {
  const { o } = nueva();
  enLinea(o);
  const { id } = o.alHttp('POST', 'canciones', MIA).cuerpo;
  assert.match(id, /^m-[0-9a-f]{10}$/);
  const r = o.alHttp('POST', 'modo', { modo: 'cancion', cancion: 'mi CANCIÓN' });
  const msgs = r.mqtt.map((m) => JSON.parse(m.payload));
  assert.ok(r.mqtt.every((m) => m.topic === 'orquesta/control/cancion' && m.payload.length < 256));
  assert.deepStrictEqual(msgs[0], { carga: 1, nombre: id, pulso: 250 });
  const total = [0, 0, 0, 0];
  for (const m of msgs.slice(1, -1)) {
    assert.strictEqual(m.desde, total[m.parte]);
    total[m.parte] += m.pasos.split(',').length;
  }
  assert.strictEqual(msgs.at(-1).totales, total.join(','));
  assert.strictEqual(total[2], 40);
  assert.deepStrictEqual(o.alMqtt('orquesta/control/cancion/ack', '{"carga":99,"ok":true}').mqtt, []);
  const a = o.alMqtt('orquesta/control/cancion/ack', '{"carga":1,"ok":true}');
  assert.deepStrictEqual(JSON.parse(a.mqtt[0].payload), { id: r.cuerpo.id, modo: 'cancion', cancion: id });
  o.alMqtt('orquesta/control/modo/ack', JSON.stringify({ id: r.cuerpo.id, ok: true, modo: 'cancion' }));
  assert.strictEqual(o.alConectar()[0].cancion, 'Mi canción');
  // Ya la tiene: no se vuelve a mandar… salvo que se edite.
  assert.strictEqual(o.alHttp('POST', 'modo', { modo: 'cancion', cancion: 'Mi canción' }).mqtt.length, 1);
  assert.strictEqual(o.alHttp('POST', 'canciones/editar', { ...MIA, id, pulso_ms: 300 }).codigo, 200);
  assert.ok(o.alHttp('POST', 'modo', { modo: 'cancion', cancion: 'Mi canción' }).mqtt.length > 1);
});

test('canción que no llega: reintenta una vez y avisa a la pantalla', () => {
  const { o, reloj } = nueva();
  enLinea(o);
  o.alHttp('POST', 'canciones', MIA);
  const r = o.alHttp('POST', 'modo', { modo: 'cancion', cancion: MIA.nombre });
  const otra = o.alMqtt('orquesta/control/cancion/ack', '{"carga":1,"ok":false}');
  assert.strictEqual(JSON.parse(otra.mqtt[0].payload).carga, 2);
  reloj.t += 2100;
  const fin = o.alSegundo();
  assert.deepStrictEqual(fin.mqtt, []);
  assert.ok(fin.ws.some((m) => m.tipo === 'modo' && m.id === r.cuerpo.id && m.ok === false));
});

test('canciones: crear, repetir nombre, incluidas y borrar', () => {
  const { o } = nueva();
  enLinea(o);
  const p = (ruta, d) => o.alHttp('POST', ruta, d);
  assert.strictEqual(o.alHttp('GET', 'canciones').cuerpo.length, 13);
  assert.strictEqual(p('canciones', { ...MIA, nombre: 'estrellita' }).codigo, 400);
  assert.strictEqual(p('canciones', { ...MIA, melodia: [] }).codigo, 400);
  assert.strictEqual(p('canciones', { ...MIA, nombre: { a: 1 } }).codigo, 400);
  const { id } = p('canciones', MIA).cuerpo;
  assert.strictEqual(p('canciones/editar', { ...MIA, id }).codigo, 200); // su propio nombre no choca
  assert.strictEqual(p('canciones/editar', { ...MIA, id: 'estrellita', nombre: 'x' }).codigo, 404);
  assert.strictEqual(p('canciones/borrar', { id: 'estrellita' }).codigo, 404);
  o.datos.cancion = id;
  assert.strictEqual(p('canciones/borrar', { id }).codigo, 409);
  o.datos.cancion = 'estrellita';
  assert.strictEqual(p('canciones/borrar', { id }).codigo, 204);
  assert.strictEqual(o.alHttp('GET', 'canciones').cuerpo.length, 13);
  assert.deepStrictEqual(Object.keys(p('canciones/arreglar', MIA).cuerpo.partes), ['osito', 'ranita', 'gatito', 'pollito']);
  assert.strictEqual(p('canciones/arreglar', {}).codigo, 400);
});

test('grabar con el gatito: lo que se oyó, en la rejilla', () => {
  const { o, reloj } = nueva();
  assert.strictEqual(o.alHttp('POST', 'canciones/grabar', { accion: 'empezar' }).codigo, 503);
  enLinea(o);
  assert.strictEqual(o.alHttp('POST', 'canciones/grabar', { accion: 'empezar' }).codigo, 409); // no está en Libre
  o.datos.modo = 'libre';
  o.alHttp('POST', 'canciones/grabar', { accion: 'empezar' });
  assert.strictEqual(o.alHttp('POST', 'canciones/grabar', { accion: 'terminar' }).codigo, 409); // sin notas
  o.alHttp('POST', 'canciones/grabar', { accion: 'empezar' });
  let seq = 0;
  const toca = (n, vel) => { seq += 1; return o.alMqtt('orquesta/gatito/evento', sobre('gatito', seq, { tipo: 'nota', nota: n, vel })); };
  assert.ok(toca('do', 100).ws.some((m) => m.tipo === 'grabacion' && m.notas === 1));
  reloj.t += 480; toca('do', 0);
  reloj.t += 30; toca('fa', 100); // fa suena sol (67)
  reloj.t += 250; toca('fa', 0);
  reloj.t += 740; toca('si', 100);
  reloj.t += 1000; toca('si', 0);
  const r = o.alHttp('POST', 'canciones/grabar', { accion: 'terminar' });
  assert.deepStrictEqual(r.cuerpo, { melodia: [[60, 2], [67, 1], [0, 3], [74, 4]], pulso_ms: 250 });
  o.alHttp('POST', 'canciones/grabar', { accion: 'empezar' });
  reloj.t += 61000;
  assert.ok(o.alSegundo().ws.some((m) => m.tipo === 'grabacion' && m.activa === false));
});

test('panel y teclas', () => {
  const { o } = nueva();
  assert.strictEqual(o.alHttp('POST', 'tecla', { tecla: '8' }).codigo, 503);
  enLinea(o);
  for (const t of ['z', '', '12', 8, null]) assert.strictEqual(o.alHttp('POST', 'tecla', { tecla: t }).codigo, 400);
  const r = o.alHttp('POST', 'tecla', { tecla: 'd' });
  assert.deepStrictEqual([r.codigo, r.urgente, r.mqtt[0].payload], [204, true, '{"tecla":"d"}']);
  assert.deepStrictEqual(o.alMqtt('orquesta/osito/tecla', '{"tecla":"#"}').ws, [{ tipo: 'tecla', tecla: '#' }]);
  assert.deepStrictEqual(o.alMqtt('orquesta/osito/tecla', '{"tecla":"<"}').ws, []);
  const w = o.alMqtt('orquesta/osito/panel', '{"tempo":110,"eco":true,"ritmo":"pop","acordes":false,"loop":"sonando","capas":2,"callados":8,"nivel":3,"avance":45}').ws;
  assert.deepStrictEqual(w[0].callados, ['pollito']);
  assert.deepStrictEqual(w[1], { tipo: 'cancion', progreso: 0.45 });
  assert.ok(o.alConectar().some((m) => m.tipo === 'panel'));
});

test('a la nube nunca iría el nombre ni la nota', () => {
  const { paraNube } = require('../almacen.js');
  const t = JSON.stringify(paraNube({ ninos: [{ alias: 'A-01', nombre: 'Ana' }], sesiones: [{ alias: 'A-01', nota: 'Ana mejoró' }], canciones: [] }));
  assert.ok(!t.includes('Ana') && t.includes('A-01'));
});
