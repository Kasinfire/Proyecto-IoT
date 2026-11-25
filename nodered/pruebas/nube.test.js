// SIMULACIÓN: Supabase falso (un fetch que responde lo que diga cada prueba), sin internet.
'use strict';
const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { Orquesta } = require('../orquesta.js');
const { NubeSupabase } = require('../nube.js');

const CLAVE = '00112233445566778899aabbccddeeff';

// Guarda cada petición; `estado` decide la respuesta (número = código HTTP, 'caida' = sin red).
function supabaseFalso() {
  const falso = { estado: 201, peticiones: [] };
  falso.fetch = async (url, opciones) => {
    if (falso.estado === 'caida') throw new Error('getaddrinfo ENOTFOUND');
    falso.peticiones.push({ url, metodo: opciones.method, cuerpo: opciones.body && JSON.parse(opciones.body), headers: opciones.headers });
    const estado = falso.estado;
    return { ok: estado < 300, status: estado, text: async () => '{"message":"error"}' };
  };
  return falso;
}

function nueva(extra = {}) {
  const reloj = { t: Date.now() };
  const avisos = [];
  const falso = supabaseFalso();
  const nube = new NubeSupabase({ url: 'https://x.supabase.co/', llave: 'sb_secret_prueba', fetch: falso.fetch });
  const o = new Orquesta({ claveHex: CLAVE, ahora: () => reloj.t, aviso: (a) => avisos.push(a), nube, ...extra });
  return { o, reloj, avisos, falso };
}

function sesionCompleta(o, alias = 'A-07', nota = 'Se llama Ana y hoy lloró') {
  o.alHttp('POST', 'ninos', { alias, nombre: 'Ana López' });
  o.alHttp('POST', 'sesion/iniciar', { alias });
  o.alHttp('POST', 'sesion/terminar', { nota });
}

test('sin internet se acumula; al volver se sube en orden y sin el nombre ni la nota', async () => {
  const { o, reloj, falso, avisos } = nueva();
  falso.estado = 'caida';
  sesionCompleta(o);
  await o.sincronizarNube();
  assert.strictEqual(o.datos.paraLaNube.length, 2);
  assert.ok(avisos.some((a) => a.includes('sin conexión')));

  falso.estado = 201;
  await o.sincronizarNube();
  assert.strictEqual(falso.peticiones.length, 0, 'espera antes de reintentar');
  reloj.t += 10000;
  const r = await o.sincronizarNube();
  assert.strictEqual(o.datos.paraLaNube.length, 0);
  assert.deepStrictEqual(falso.peticiones.map((p) => p.url), [
    'https://x.supabase.co/rest/v1/ninos?on_conflict=alias',
    'https://x.supabase.co/rest/v1/sesiones?on_conflict=id',
  ]);
  const todo = JSON.stringify(falso.peticiones.map((p) => p.cuerpo));
  assert.ok(!todo.includes('Ana'), 'ni el nombre ni la nota salen de la laptop');
  const [sesion] = falso.peticiones[1].cuerpo;
  assert.strictEqual(sesion.alias, 'A-07');
  assert.match(sesion.id, /^[0-9a-f-]{36}$/);
  assert.match(sesion.inicio, /^\d{4}-\d\d-\d\dT/);
  assert.strictEqual(r.ws[0].nube_pendientes, 0);
});

test('la espera crece hasta 60 s y vuelve a 10 s al funcionar', async () => {
  const { o, reloj, falso } = nueva();
  falso.estado = 503; // proyecto pausado
  sesionCompleta(o);
  const esperas = [];
  for (let i = 0; i < 5; i += 1) {
    const antes = reloj.t;
    await o.sincronizarNube();
    esperas.push(o.nubeReintentoEn - antes);
    reloj.t = o.nubeReintentoEn;
  }
  assert.deepStrictEqual(esperas, [10000, 20000, 40000, 60000, 60000]);
  falso.estado = 201;
  await o.sincronizarNube();
  assert.strictEqual(o.datos.paraLaNube.length, 0);
  assert.strictEqual(o.nubeEspera, 10000);
});

test('una llave mal puesta (401) no tira nada; un dato inválido (400) se aparta y avisa', async () => {
  const { o, reloj, falso, avisos } = nueva();
  falso.estado = 401;
  sesionCompleta(o);
  await o.sincronizarNube();
  assert.strictEqual(o.datos.paraLaNube.length, 2);
  falso.estado = 400;
  reloj.t += 60000;
  await o.sincronizarNube();
  assert.strictEqual(o.datos.paraLaNube.length, 0);
  assert.ok(avisos.some((a) => a.includes('rechazó')));
  assert.deepStrictEqual(o.datos.rechazadasNube.map((r) => r.op.op), ['nino', 'sesion']);
  assert.ok(!JSON.stringify(o.datos.rechazadasNube).includes('Ana'));
});

test('tablas sin crear (404): no se pierde nada y se sube al crearlas', async () => {
  const { o, reloj, falso } = nueva();
  falso.estado = 404;
  sesionCompleta(o);
  await o.sincronizarNube();
  assert.strictEqual(o.datos.paraLaNube.length, 2);
  assert.strictEqual(o.datos.rechazadasNube.length, 0);
  falso.estado = 201;
  reloj.t += 10000;
  await o.sincronizarNube();
  assert.strictEqual(o.datos.paraLaNube.length, 0);
});

test('si falta el niño de una sesión (409) se crea y la sesión se sube', async () => {
  const hechas = [];
  let hayNino = false;
  const fetch = async (url, { headers }) => {
    const tabla = url.split('/v1/')[1].split('?')[0];
    hechas.push(`${tabla} ${headers.Prefer.split(',')[0]}`);
    if (tabla === 'ninos') hayNino = true;
    const estado = tabla === 'sesiones' && !hayNino ? 409 : 201;
    return { ok: estado < 300, status: estado, text: async () => 'llave foranea' };
  };
  const nube = new NubeSupabase({ url: 'https://x', llave: 'sb_secret_x', fetch });
  const sesion = { id: 'x', alias: 'A-01', inicio: 1, fin: 2, modos: [], cancion: 'c', resumen: {} };
  assert.strictEqual((await nube.enviar({ op: 'sesion', sesion })).resultado, 'ok');
  assert.deepStrictEqual(hechas, ['sesiones resolution=merge-duplicates', 'ninos resolution=ignore-duplicates', 'sesiones resolution=merge-duplicates']);
});

test('lo apartado sobrevive a reiniciar, cambia de alias con el niño y se borra con él', async () => {
  const archivo = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'orq-')), 'o.json');
  const a = nueva({ archivo });
  a.falso.estado = 400;
  sesionCompleta(a.o, 'A-07');
  await a.o.sincronizarNube();
  const b = nueva({ archivo });
  assert.strictEqual(b.o.datos.rechazadasNube.length, 2);
  b.o.alHttp('POST', 'ninos/editar', { alias: 'A-07', nuevo_alias: 'A-08', nombre: '' });
  assert.ok(!JSON.stringify(b.o.datos.rechazadasNube).includes('A-07'));
  b.o.alHttp('POST', 'ninos/borrar', { alias: 'A-08' });
  assert.strictEqual(b.o.datos.rechazadasNube.length, 0);
});

test('editar y borrar niños: renombrar, avatar y borrado en cascada', async () => {
  const { o, falso } = nueva();
  o.alHttp('POST', 'ninos', { alias: 'B-01' });
  o.alHttp('POST', 'ninos/editar', { alias: 'B-01', nuevo_alias: 'B-02', nombre: 'x' });
  o.alHttp('POST', 'ninos/borrar', { alias: 'B-02' });
  await o.sincronizarNube();
  assert.deepStrictEqual(falso.peticiones.map((p) => `${p.metodo} ${p.url.split('/v1/')[1]}`), [
    'POST ninos?on_conflict=alias',
    'PATCH ninos?alias=eq.B-01',
    'POST ninos?on_conflict=alias',
    'DELETE ninos?alias=eq.B-02',
  ]);
  assert.deepStrictEqual(falso.peticiones[1].cuerpo, { alias: 'B-02' });
});

test('lo pendiente sobrevive a reiniciar Node-RED, y lo de antes de Supabase se sube una vez', async () => {
  const archivo = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'orq-')), 'o.json');
  // Antes de configurar Supabase.
  const { o: sinNube } = { o: new Orquesta({ claveHex: CLAVE, archivo }) };
  sesionCompleta(sinNube, 'C-03');
  // Primera vez con Supabase, sin internet.
  const a = nueva({ archivo });
  a.falso.estado = 'caida';
  await a.o.sincronizarNube();
  assert.strictEqual(a.o.datos.paraLaNube.length, 2);
  // Se reinicia Node-RED y ya hay internet.
  const b = nueva({ archivo });
  assert.strictEqual(b.o.datos.paraLaNube.length, 2, 'no se vuelve a encolar lo mismo');
  await b.o.sincronizarNube();
  assert.strictEqual(b.falso.peticiones.length, 2);
  assert.strictEqual(b.o.datos.paraLaNube.length, 0);
});

test('sin Supabase configurado no se encola nada', async () => {
  const o = new Orquesta({ claveHex: CLAVE });
  sesionCompleta(o);
  assert.deepStrictEqual(o.datos.paraLaNube, []);
  assert.strictEqual((await o.sincronizarNube()).ws.length, 0);
  assert.ok(!('nube_pendientes' in o.red()));
});

test('las llaves nuevas van solo en apikey; las viejas (JWT) también en Authorization', async () => {
  const falso = supabaseFalso();
  await new NubeSupabase({ url: 'https://x.supabase.co', llave: 'sb_secret_x', fetch: falso.fetch }).enviar({ op: 'borrarNino', alias: 'A-01' });
  await new NubeSupabase({ url: 'https://x.supabase.co', llave: 'eyJhbGciOi', fetch: falso.fetch }).enviar({ op: 'borrarNino', alias: 'A-01' });
  assert.ok(!('Authorization' in falso.peticiones[0].headers));
  assert.strictEqual(falso.peticiones[1].headers.Authorization, 'Bearer eyJhbGciOi');
});
