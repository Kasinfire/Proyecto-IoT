// Puente entre el broker y la interfaz web: descifra los eventos del osito, confirma los que ya
// se guardaron, lleva la sesión y arma los mensajes que esperan las pantallas.
// No depende de Node-RED: los flujos solo lo llaman (así se prueba con `node --test`).
'use strict';

const crypto = require('node:crypto');
const { AlmacenArchivo } = require('./almacen.js');
const arreglo = require('./arreglo.js');
const { leerAvatar } = require('./avatar.js');
const { PULSO_MS: PULSO_GRABACION_MS, aMelodia } = require('./grabacion.js');

const NODOS = ['osito', 'ranita', 'gatito', 'pollito'];
const ANIMALES = ['gatito', 'ranita', 'pollito'];
const MODOS = ['cancion', 'libre', 'simon'];
const INCLUIDAS = arreglo.incluidas(); // las trae el osito en su firmware
const CANCION_DIAGNOSTICO = 'prueba';
const CANCIONES_MAX = 200;
const TECLAS = '123A456B789C*0#Dd';
const PASOS_POR_TROZO = 16; // cabe en los 256 bytes que acepta el osito
const ESPERA_CARGA_MS = 2000;
const INTENTOS_CARGA = 2;
const GRABAR_MAX_MS = 60000;
const ALIAS = /^([A-Z]{1,2})-?([0-9]{1,3})$/;
const NOMBRE_MAX = 40;
const NONCE_BYTES = 12;
const TAG_BYTES = 16;
const VOLUMEN_MAX = 127;
const ACTIVO_MS = 2000;
const MINIMO_JUNTOS = 3;
// El osito vuelve a publicar lo que no se le confirma. Si después de este tiempo sigue faltando
// un seq, ya no lo tiene (lo descartó): se sigue sin él.
const ESPERA_HUECO_MS = 25000;
const ESPERA_COLA_MS = 5000;
const RECIBIDOS_MAX = 512;
const DIAS = { mes: 31, trimestre: 92, todo: Infinity };

// { seq, data } -> evento. Lanza un error si la clave, el tópico o el seq no son los que se cifraron.
function descifrar(clave, nodo, sobre) {
  const bytes = Buffer.from(String(sobre.data), 'base64');
  if (!Number.isInteger(sobre.seq) || bytes.length <= NONCE_BYTES + TAG_BYTES) {
    throw new Error('sobre incompleto');
  }
  const d = crypto.createDecipheriv('aes-128-gcm', clave, bytes.subarray(0, NONCE_BYTES),
    { authTagLength: TAG_BYTES });
  d.setAAD(Buffer.from(`${nodo}:${sobre.seq}`));
  d.setAuthTag(bytes.subarray(bytes.length - TAG_BYTES));
  const texto = Buffer.concat([d.update(bytes.subarray(NONCE_BYTES, bytes.length - TAG_BYTES)), d.final()]);
  const evento = JSON.parse(texto.toString('utf8'));
  if (evento.nodo !== nodo || evento.seq !== sobre.seq) throw new Error('el evento no coincide con su sobre');
  return evento;
}

// 'a07' y 'A-07' son el mismo alias. null si no tiene la forma de un alias.
function normalizarAlias(texto) {
  const m = ALIAS.exec(String(texto ?? '').trim().toUpperCase());
  return m ? `${m[1]}-${m[2].padStart(2, '0')}` : null;
}

// null si es demasiado largo. El nombre es opcional y nunca sale de esta laptop: las sesiones, el
// broker y la nube solo llevan el alias (orquesta-proyecto.md, sección 7).
function limpiarNombre(texto) {
  const nombre = String(texto ?? '').replace(/[\u0000-\u001f\u007f]/g, ' ').replace(/\s+/g, ' ').trim();
  return nombre.length <= NOMBRE_MAX ? nombre : null;
}

const resumenVacio = () => ({
  gestos: { gatito: 0, ranita: 0, pollito: 0 }, simon: { aciertos: 0, turnos: 0 }, juntos_s: 0,
});

class Orquesta {
  constructor({ claveHex, archivo, almacen = new AlmacenArchivo(archivo), ahora = Date.now, aviso = () => {} }) {
    if (!/^[0-9a-fA-F]{32}$/.test(claveHex || '')) {
      throw new Error('MQTT_AES_KEY_HEX debe tener 32 caracteres hex');
    }
    this.clave = Buffer.from(claveHex, 'hex');
    this.almacen = almacen;
    this.ahora = ahora;
    this.aviso = aviso;
    // Lo que se conserva al reiniciar Node-RED.
    this.datos = {
      ninos: [], sesiones: [], sesion: null, siguienteId: 1, avisoSilenciado: false,
      modo: 'cancion', cancion: INCLUIDAS[0].id, canciones: [], siguienteCarga: 1, sync: { arranque: null, hasta: 0, recibidos: [] },
    };
    this.conectado = {};
    this.desconectadoDesde = {};
    this.broker = null; // null = todavía no se sabe
    this.ultimoGesto = {};
    this.sombra = 0;
    this.volumen = null;
    this.ositoEnLinea = null;
    this.pendientes = 0;
    this.volvioEn = null; // reconectado, falta que diga cuántos eventos traía
    this.huecoDesde = null;
    this.pedidos = new Map(); // id -> canción pedida
    this.carga = null; // canción que se le está mandando al osito
    this.cancionCambiada = false; // se editó la que está sonando
    this.panel = null;
    this.grabacion = null;
    this.guardado = null;
    this.cargar();
  }

  cargar() {
    const leido = this.almacen.leer();
    if (leido) Object.assign(this.datos, leido);
    // Antes se guardaba el nombre tal cual lo mandaba el osito; ahora es el id.
    if (!this.buscarCancion(this.datos.cancion) && this.datos.cancion !== CANCION_DIAGNOSTICO) {
      this.datos.cancion = INCLUIDAS[0].id;
    }
  }

  guardar() {
    this.almacen.guardar(this.datos);
    this.guardado = this.ahora();
  }

  // ---------- MQTT ----------

  // Devuelve { ws: [mensajes], mqtt: [{ topic, payload }] }.
  alMqtt(topic, payload) {
    const salida = { ws: [], mqtt: [] };
    let datos;
    try {
      datos = JSON.parse(String(payload));
    } catch {
      this.aviso(`Mensaje que no es JSON en ${topic}`);
      return salida;
    }
    if (datos === null || typeof datos !== 'object') {
      this.aviso(`Mensaje vacío en ${topic}`);
      return salida;
    }
    const [, nodo, cual, tercero] = topic.split('/');
    if (topic === 'orquesta/control/modo/ack') this.alAckModo(datos, salida);
    else if (topic === 'orquesta/control/cancion/ack') this.alAckCancion(datos, salida);
    else if (!NODOS.includes(nodo) || tercero) return salida;
    else if (cual === 'estado') this.alEstado(nodo, datos, salida);
    else if (cual === 'evento') this.alSobre(nodo, datos, salida);
    else if (topic === 'orquesta/osito/volumen') this.alVolumen(datos, salida);
    else if (topic === 'orquesta/osito/panel') this.alPanel(datos, salida);
    else if (topic === 'orquesta/osito/tecla') this.alTecla(datos, salida);
    else if (topic === 'orquesta/osito/cola') this.alCola(datos, salida);
    return salida;
  }

  alEstado(nodo, datos, salida) {
    const conectado = datos.estado === 'online';
    if (nodo === 'osito') {
      // Al volver, el osito manda enseguida su cola: hasta entonces se sigue mostrando la caída.
      if (this.ositoEnLinea === false && conectado) this.volvioEn = this.ahora();
      if (!conectado) this.volvioEn = null;
      // Sin osito nadie sabe cómo están los animales: lo retenido en el broker ya no vale. Al
      // volver, el osito manda otra vez el estado de todos.
      if (!conectado) salida.ws.push(...ANIMALES.map((n) => ({ tipo: 'estado', nodo: n, conectado: null })));
      this.ositoEnLinea = conectado;
      if (this.volvioEn === null) salida.ws.push(this.red());
    } else if (this.conectado[nodo] !== false && !conectado && this.datos.sesion) {
      this.datos.sesion.aviso = `${nodo[0].toUpperCase()}${nodo.slice(1)} se desconectó`;
    }
    // Lo que ya estaba fuera al arrancar Node-RED no se sabe desde cuándo.
    if (!conectado && this.conectado[nodo] === true) this.desconectadoDesde[nodo] = this.ahora();
    if (conectado) delete this.desconectadoDesde[nodo];
    this.conectado[nodo] = conectado;
    salida.ws.push(this.mensajeEstado(nodo));
  }

  mensajeEstado(nodo) {
    const msg = { tipo: 'estado', nodo, conectado: this.estadoDe(nodo) };
    if (msg.conectado === false && this.desconectadoDesde[nodo]) msg.desde = this.desconectadoDesde[nodo];
    return msg;
  }

  // Node-RED perdió o recuperó el broker. Sin broker no se sabe nada del osito ni de los animales;
  // al volver, el broker manda lo retenido.
  alBroker(conectado) {
    const salida = { ws: [], mqtt: [] };
    if (this.broker === conectado) return salida;
    this.broker = conectado;
    if (!conectado) {
      this.conectado = {};
      this.desconectadoDesde = {};
      this.ositoEnLinea = null;
      this.volvioEn = null;
      salida.ws.push(...NODOS.map((nodo) => ({ tipo: 'estado', nodo, conectado: null })));
    }
    salida.ws.push(this.red());
    return salida;
  }

  sinOsito() {
    return this.broker === false ? 'no hay conexión con el broker' : 'el osito no está en línea';
  }

  // null = no se sabe (el osito no está en línea).
  estadoDe(nodo) {
    return nodo !== 'osito' && this.ositoEnLinea === false ? null : this.conectado[nodo];
  }

  alVolumen(datos, salida) {
    if (!Number.isFinite(datos.valor)) return;
    this.volumen = Math.round((Math.min(Math.max(datos.valor, 0), VOLUMEN_MAX) * 100) / VOLUMEN_MAX);
    salida.ws.push({ tipo: 'volumen', nodo: 'osito', valor: this.volumen });
  }

  alCola(datos, salida) {
    if (!Number.isInteger(datos.pendientes)) return;
    this.pendientes = datos.pendientes;
    const volvio = this.volvioEn !== null;
    this.volvioEn = null;
    salida.ws.push(this.red(volvio));
  }

  red(volvio = false) {
    const msg = { tipo: 'red', internet: this.broker !== false && this.ositoEnLinea !== false && this.volvioEn === null, pendientes: this.pendientes, broker: this.broker !== false };
    if (volvio) msg.recuperados = this.pendientes;
    if (this.guardado) msg.guardado = this.guardado;
    return msg;
  }

  alAckModo(datos, salida) {
    if (!MODOS.includes(datos.modo)) return;
    if (datos.ok) {
      this.datos.modo = datos.modo;
      const pedida = this.pedidos.get(datos.id);
      if (pedida) {
        this.datos.cancion = pedida;
        this.cancionCambiada = false;
      }
      this.anotarModo(datos.modo);
      this.guardar();
    }
    this.pedidos.delete(datos.id);
    salida.ws.push({ tipo: 'modo', id: datos.id, ok: Boolean(datos.ok), modo: this.datos.modo, cancion: this.nombreCancion() });
  }

  alPanel(datos, salida) {
    if (!Number.isFinite(datos.tempo)) return;
    const n = (x) => (Number.isFinite(x) ? x : 0);
    const { tempo, eco, ritmo, acordes, loop, capas, callados, nivel, avance } = datos;
    this.panel = {
      tipo: 'panel', tempo, eco: eco === true, ritmo: String(ritmo), acordes: acordes === true, loop: String(loop),
      capas: n(capas), callados: NODOS.filter((_, i) => n(callados) & (1 << i)), nivel: n(nivel),
    };
    salida.ws.push(this.panel);
    if (this.datos.modo === 'cancion') salida.ws.push({ tipo: 'cancion', progreso: n(avance) / 100 });
  }

  alTecla(datos, salida) {
    if (typeof datos.tecla === 'string' && datos.tecla.length === 1 && TECLAS.includes(datos.tecla)) {
      salida.ws.push({ tipo: 'tecla', tecla: datos.tecla });
    }
  }

  // ---------- Canciones ----------

  todas() {
    return [...INCLUIDAS, ...this.datos.canciones];
  }

  buscarCancion(id) {
    return this.todas().find((c) => c.id === id);
  }

  // Las pantallas usan el nombre; por MQTT viaja el id.
  porNombre(nombre) {
    const buscado = String(nombre ?? '').trim().toLocaleLowerCase('es');
    return this.todas().find((c) => c.nombre.toLocaleLowerCase('es') === buscado);
  }

  nombreCancion(id = this.datos.cancion) {
    return this.buscarCancion(id)?.nombre ?? id;
  }

  // Los mensajes que le mandan una canción al osito.
  trozos(cancion, carga) {
    const partes = arreglo.arreglar(cancion);
    const msgs = [{ carga, nombre: cancion.id, pulso: cancion.pulso_ms }];
    arreglo.PARTES.forEach((parte, k) => {
      for (let i = 0; i < partes[parte].length; i += PASOS_POR_TROZO) {
        const pasos = partes[parte].slice(i, i + PASOS_POR_TROZO).map(([n, d, v = 0]) => `${n}.${d}.${v}`).join(',');
        msgs.push({ carga, parte: k, desde: i, pasos });
      }
    });
    msgs.push({ carga, totales: arreglo.PARTES.map((p) => partes[p].length).join(',') });
    return msgs.map((m) => ({ topic: 'orquesta/control/cancion', payload: JSON.stringify(m) }));
  }

  mandarCancion(cancion, pedido, intento = 1) {
    const numero = this.datos.siguienteCarga;
    this.datos.siguienteCarga += 1;
    this.carga = { numero, cancion, pedido, intento, desde: this.ahora() };
    return this.trozos(cancion, numero);
  }

  alAckCancion(datos, salida) {
    if (!this.carga || datos.carga !== this.carga.numero) return;
    const { pedido } = this.carga;
    if (datos.ok) {
      this.carga = null;
      salida.mqtt.push({ topic: 'orquesta/control/modo', payload: JSON.stringify(pedido) });
    } else {
      this.reintentarCarga(salida);
    }
  }

  reintentarCarga(salida) {
    const { cancion, pedido, intento } = this.carga;
    if (intento < INTENTOS_CARGA && this.ositoEnLinea) {
      salida.mqtt.push(...this.mandarCancion(cancion, pedido, intento + 1));
      return;
    }
    this.carga = null;
    this.pedidos.delete(pedido.id);
    this.aviso(`El osito no pudo recibir la canción ${cancion.id}`);
    salida.ws.push({ tipo: 'modo', id: pedido.id, ok: false, modo: this.datos.modo, cancion: this.nombreCancion() });
  }

  // Para la pantalla: con la melodía, para poder duplicarla o editarla.
  fichaCancion(c) {
    return {
      id: c.id, nombre: c.nombre, tipo: c.tipo, duracion: arreglo.duracion(c), incluida: Boolean(c.incluida),
      melodia: c.melodia, compas: c.compas, estilo: c.estilo, pulso_ms: c.pulso_ms,
    };
  }

  // { cancion } ya limpia, o { error }.
  leerCancion(cuerpo, propia = null) {
    const c = {
      nombre: typeof cuerpo.nombre === 'string' ? cuerpo.nombre.replace(/\s+/g, ' ').trim() : '', tipo: cuerpo.tipo, estilo: cuerpo.estilo,
      compas: cuerpo.compas, pulso_ms: cuerpo.pulso_ms, melodia: cuerpo.melodia,
    };
    const error = arreglo.revisar(c);
    if (error) return { error };
    const otra = this.porNombre(c.nombre);
    if (otra && otra !== propia) return { error: 'ya existe una canción con ese nombre' };
    return { cancion: c };
  }

  alSobre(nodo, sobre, salida) {
    let evento;
    try {
      evento = descifrar(this.clave, nodo, sobre);
    } catch (error) {
      this.aviso(`Evento rechazado de ${nodo}: ${error.message}`); // alterado, o la clave no es la misma
      return;
    }
    const sync = this.datos.sync;
    if (sync.arranque !== (evento.arranque ?? 0)) {
      // El osito se reinició: sus seq empiezan otra vez.
      this.datos.sync = { arranque: evento.arranque ?? 0, hasta: evento.seq - 1, recibidos: [] };
      this.huecoDesde = null;
    }
    const s = this.datos.sync;
    if (evento.seq > s.hasta && !s.recibidos.includes(evento.seq)) {
      s.recibidos.push(evento.seq);
      if (s.recibidos.length > RECIBIDOS_MAX) s.recibidos.shift();
      this.alEvento(evento, salida);
    }
    this.avanzar();
    // Primero se guarda y después se confirma: si Node-RED se apaga en medio, el osito lo repite.
    this.guardar();
    this.confirmar(salida);
  }

  // Mueve `hasta` mientras no falte ningún seq.
  avanzar() {
    const s = this.datos.sync;
    const quitar = new Set();
    while (s.recibidos.includes(s.hasta + 1)) {
      s.hasta += 1;
      quitar.add(s.hasta);
    }
    s.recibidos = s.recibidos.filter((n) => !quitar.has(n) && n > s.hasta);
    if (s.recibidos.length === 0) this.huecoDesde = null;
    else if (this.huecoDesde === null || quitar.size) this.huecoDesde = this.ahora();
  }

  confirmar(salida) {
    if (this.datos.sync.hasta > 0) {
      salida.mqtt.push({ topic: 'orquesta/sync/ack', payload: JSON.stringify({ hasta_seq: this.datos.sync.hasta }) });
    }
  }

  alEvento(evento, salida) {
    const { nodo } = evento;
    const sesion = this.datos.sesion;
    switch (evento.tipo) {
      case 'nota':
        this.anotarGrabacion(evento, salida);
        if (evento.vel > 0) {
          this.ultimoGesto[nodo] = this.ahora();
          if (sesion) sesion.resumen.gestos.gatito += 1;
        }
        salida.ws.push({ tipo: 'nota', nodo, nota: evento.nota, vel: evento.vel });
        break;
      case 'sombra':
        if (evento.nivel > 0) {
          this.ultimoGesto[nodo] = this.ahora();
          if (sesion && this.sombra === 0) sesion.resumen.gestos.ranita += 1;
        }
        this.sombra = evento.nivel;
        salida.ws.push({ tipo: 'sombra', nodo, nivel: evento.nivel });
        break;
      case 'agitar':
        this.ultimoGesto[nodo] = this.ahora();
        if (sesion) sesion.resumen.gestos.pollito += 1;
        salida.ws.push({ tipo: 'agitar', nodo, intensidad: evento.intensidad });
        break;
      case 'modo':
        if (!MODOS.includes(evento.modo)) break;
        this.datos.modo = evento.modo;
        this.datos.cancion = evento.cancion;
        if (evento.origen === 'arranque') this.cancionCambiada = false;
        this.anotarModo(evento.modo);
        // El que pidió Node-RED ya llegó por el acuse, con su id.
        if (evento.origen !== 'nodered') {
          salida.ws.push({ tipo: 'modo', id: null, ok: true, modo: evento.modo, cancion: this.nombreCancion() });
        }
        break;
      case 'simon':
        if (evento.resultado) {
          if (sesion) {
            sesion.resumen.simon.turnos += 1;
            if (evento.resultado === 'acierto') sesion.resumen.simon.aciertos += 1;
          }
          salida.ws.push({ tipo: 'simon', nodo: evento.pedido, resultado: evento.resultado, nivel: evento.nivel });
        } else {
          salida.ws.push({ tipo: 'simon', nodo: evento.pedido, nivel: evento.nivel });
        }
        break;
      default:
        break;
    }
  }

  anotarGrabacion(evento, salida) {
    if (!this.grabacion) return;
    const { toques } = this.grabacion;
    for (const x of toques) if (x.nota === evento.nota && x.fin === null) x.fin = this.ahora();
    if (evento.vel > 0) {
      toques.push({ nota: evento.nota, inicio: this.ahora(), fin: null });
      salida.ws.push({ tipo: 'grabacion', activa: true, notas: toques.length });
    }
  }

  // Cada segundo: cuenta el tiempo tocando juntos y deja de esperar un seq que ya no existe.
  alSegundo() {
    const salida = { ws: [], mqtt: [] };
    const t = this.ahora();
    const tocando = ANIMALES.filter((n) => this.conectado[n] !== false && t - (this.ultimoGesto[n] ?? -Infinity) < ACTIVO_MS).length;
    if (this.datos.sesion && tocando + (this.conectado.osito ? 1 : 0) >= MINIMO_JUNTOS) {
      this.datos.sesion.resumen.juntos_s += 1;
    }
    if (this.volvioEn !== null && t - this.volvioEn >= ESPERA_COLA_MS) {
      this.volvioEn = null;
      salida.ws.push(this.red());
    }
    if (this.carga && t - this.carga.desde >= ESPERA_CARGA_MS) this.reintentarCarga(salida);
    if (this.grabacion && t - this.grabacion.desde >= GRABAR_MAX_MS && !this.grabacion.llena) {
      this.grabacion.llena = true; // la pantalla la termina
      salida.ws.push({ tipo: 'grabacion', activa: false, notas: this.grabacion.toques.length });
    }
    if (this.huecoDesde !== null && t - this.huecoDesde >= ESPERA_HUECO_MS) {
      const s = this.datos.sync;
      const sigue = Math.min(...s.recibidos);
      this.aviso(`El osito ya no tiene los eventos ${s.hasta + 1} a ${sigue - 1}`);
      s.hasta = sigue - 1;
      this.avanzar();
      this.guardar();
      this.confirmar(salida);
    }
    return salida;
  }

  // ---------- Pantallas ----------

  // Lo que recibe una pantalla al conectarse, para no empezar en cero.
  alConectar() {
    const ws = [this.mensajeSesion()];
    for (const nodo of NODOS) {
      if (nodo in this.conectado) ws.push(this.mensajeEstado(nodo));
    }
    ws.push(this.red());
    if (this.panel && this.ositoEnLinea) ws.push(this.panel);
    if (this.volumen !== null) ws.push({ tipo: 'volumen', nodo: 'osito', valor: this.volumen });
    return ws;
  }

  mensajeSesion() {
    const s = this.datos.sesion;
    return {
      tipo: 'sesion', alias: s?.alias ?? null, inicio: s?.inicio ?? null, modo: this.datos.modo,
      cancion: this.nombreCancion(), resumen: s?.resumen ?? resumenVacio(), silenciado: this.datos.avisoSilenciado,
    };
  }

  anotarModo(modo) {
    const s = this.datos.sesion;
    if (s && !s.modos.includes(modo)) s.modos.push(modo);
  }

  // { codigo, cuerpo, ws, mqtt }. El MQTT sale enseguida: la pantalla recuerda los acuses que lleguen
  // antes que su id.
  alHttp(metodo, ruta, datos = {}) {
    const resp = (codigo, cuerpo = null, ws = [], mqtt = []) => ({ codigo, cuerpo, ws, mqtt });
    const error = (codigo, texto) => resp(codigo, { error: texto });
    const cuerpo = datos && typeof datos === 'object' ? datos : {};
    switch (`${metodo} ${ruta}`) {
      case 'POST modo': {
        if (!MODOS.includes(cuerpo.modo)) return error(400, 'modo desconocido');
        const pedido = { id: this.datos.siguienteId, modo: cuerpo.modo };
        let cancion = null;
        if (cuerpo.cancion) {
          cancion = this.porNombre(cuerpo.cancion);
          if (!cancion) return error(400, 'esa canción ya no está en la lista');
          // Si ya es la que tiene el osito, no hace falta mandarla otra vez.
          if (cancion.id !== this.datos.cancion || this.cancionCambiada) pedido.cancion = cancion.id;
        }
        if (!this.ositoEnLinea) return error(503, this.sinOsito());
        this.datos.siguienteId += 1;
        this.pedidos.set(pedido.id, pedido.cancion);
        // Una canción propia se manda primero; el modo, cuando el osito confirme que llegó completa.
        const mqtt = pedido.cancion && !cancion.incluida
          ? this.mandarCancion(cancion, pedido)
          : [{ topic: 'orquesta/control/modo', payload: JSON.stringify(pedido) }];
        this.guardar();
        return resp(200, { id: pedido.id }, [], mqtt);
      }
      case 'POST aviso':
        if (typeof cuerpo.silenciar !== 'boolean') return error(400, 'falta silenciar');
        if (!this.ositoEnLinea) return error(503, this.sinOsito());
        this.datos.avisoSilenciado = cuerpo.silenciar;
        this.guardar();
        return resp(204, null, [], [{ topic: 'orquesta/control/aviso', payload: JSON.stringify({ silenciar: cuerpo.silenciar }) }]);
      case 'GET sesion/actual':
        return resp(200, this.datos.sesion ? { alias: this.datos.sesion.alias, inicio: this.datos.sesion.inicio } : null);
      case 'POST sesion/iniciar': {
        const nino = this.nino(cuerpo.alias);
        if (!nino) return error(404, 'ese niño no está en la lista');
        if (this.datos.sesion) return error(409, `ya hay una sesión con ${this.datos.sesion.alias}`);
        this.datos.sesion = {
          alias: nino.alias, inicio: this.ahora(), modos: [this.datos.modo], aviso: null, resumen: resumenVacio(),
        };
        this.guardar();
        return resp(204, null, [this.mensajeSesion()]);
      }
      case 'POST sesion/terminar': {
        const s = this.datos.sesion;
        if (!s) return error(409, 'no hay una sesión abierta');
        this.datos.sesiones.push({
          alias: s.alias, inicio: s.inicio, fin: this.ahora(), modos: s.modos, cancion: this.nombreCancion(),
          resumen: s.resumen, aviso: s.aviso, nota: String(cuerpo.nota ?? '').slice(0, 2000),
        });
        this.datos.sesion = null;
        this.guardar();
        return resp(204, null, [this.mensajeSesion()]);
      }
      case 'GET canciones':
        return resp(200, this.todas().map((c) => this.fichaCancion(c)));
      case 'POST canciones': {
        if (this.datos.canciones.length >= CANCIONES_MAX) return error(409, `ya hay ${CANCIONES_MAX} canciones propias`);
        const { cancion, error: motivo } = this.leerCancion(cuerpo);
        if (motivo) return error(400, motivo);
        cancion.id = `m-${crypto.randomBytes(5).toString('hex')}`;
        this.datos.canciones.push(cancion);
        this.guardar();
        return resp(200, { id: cancion.id }, [{ tipo: 'canciones' }]);
      }
      case 'POST canciones/editar': {
        const vieja = this.datos.canciones.find((c) => c.id === cuerpo.id);
        if (!vieja) return error(404, 'esa canción no se puede editar');
        const { cancion, error: motivo } = this.leerCancion(cuerpo, vieja);
        if (motivo) return error(400, motivo);
        Object.assign(vieja, cancion);
        if (this.datos.cancion === vieja.id) this.cancionCambiada = true;
        this.guardar();
        return resp(200, { id: vieja.id }, [{ tipo: 'canciones' }]);
      }
      case 'POST canciones/borrar': {
        if (!this.datos.canciones.some((c) => c.id === cuerpo.id)) return error(404, 'esa canción no se puede borrar');
        if (this.datos.cancion === cuerpo.id && this.datos.modo === 'cancion') return error(409, 'está sonando: elige otra antes de borrarla');
        this.datos.canciones = this.datos.canciones.filter((c) => c.id !== cuerpo.id);
        this.guardar();
        return resp(204, null, [{ tipo: 'canciones' }]);
      }
      case 'POST canciones/arreglar': {
        const motivo = arreglo.revisar({ ...cuerpo, nombre: 'x', tipo: 'mias' });
        if (motivo) return error(400, motivo);
        return resp(200, { partes: arreglo.arreglar(cuerpo), pulso_ms: cuerpo.pulso_ms });
      }
      case 'POST canciones/grabar': {
        if (cuerpo.accion === 'empezar') {
          if (!this.ositoEnLinea) return error(503, this.sinOsito());
          if (this.conectado.gatito !== true) return error(409, 'el gatito no está conectado');
          if (this.datos.modo !== 'libre') return error(409, 'pon el osito en modo Libre');
          this.grabacion = { desde: this.ahora(), toques: [], llena: false };
          return resp(204);
        }
        if (!this.grabacion) return error(409, 'no se está grabando');
        const { toques } = this.grabacion;
        this.grabacion = null;
        if (cuerpo.accion === 'cancelar') return resp(204);
        const melodia = aMelodia(toques, arreglo.PASOS_MAX - 1);
        if (!melodia.length) return error(409, 'no llegó ninguna nota del gatito');
        return resp(200, { melodia, pulso_ms: PULSO_GRABACION_MS });
      }
      case 'POST tecla': {
        const { tecla } = cuerpo;
        if (typeof tecla !== 'string' || tecla.length !== 1 || !TECLAS.includes(tecla)) return error(400, 'tecla desconocida');
        if (!this.ositoEnLinea) return error(503, this.sinOsito());
        return resp(204, null, [], [{ topic: 'orquesta/control/tecla', payload: JSON.stringify({ tecla }) }]);
      }
      case 'GET ninos':
        return resp(200, this.datos.ninos.map((n) => this.fichaNino(n.alias)));
      case 'POST ninos': {
        const nombre = limpiarNombre(cuerpo.nombre);
        if (nombre === null) return error(400, `el nombre es demasiado largo (máximo ${NOMBRE_MAX} letras)`);
        const sinAlias = !String(cuerpo.alias ?? '').trim();
        if (sinAlias && !nombre) return error(400, 'escribe un nombre o un alias');
        const alias = sinAlias ? this.aliasLibre() : normalizarAlias(cuerpo.alias);
        if (!alias) return error(400, 'el alias debe ser como A-07');
        if (this.nino(alias)) return error(409, `ya existe un niño con el alias ${alias}`);
        const nino = { alias, nombre };
        if (cuerpo.avatar !== undefined) {
          nino.avatar = leerAvatar(cuerpo.avatar);
          if (!nino.avatar) return error(400, 'ese avatar no es válido');
        }
        this.datos.ninos.push(nino);
        this.guardar();
        return resp(200, { alias });
      }
      case 'POST ninos/editar': {
        const nino = this.nino(cuerpo.alias);
        if (!nino) return error(404, 'ese niño ya no está en la lista');
        const nombre = limpiarNombre(cuerpo.nombre);
        if (nombre === null) return error(400, `el nombre es demasiado largo (máximo ${NOMBRE_MAX} letras)`);
        const nuevo = normalizarAlias(cuerpo.nuevo_alias);
        if (!nuevo) return error(400, 'el alias debe ser como A-07');
        const otro = this.nino(nuevo);
        if (otro && otro !== nino) return error(409, `ya existe un niño con el alias ${nuevo}`);
        // Si no viene, se queda el que tenía.
        const avatar = cuerpo.avatar === undefined ? nino.avatar : leerAvatar(cuerpo.avatar);
        if (avatar === null) return error(400, 'ese avatar no es válido');
        const viejo = nino.alias;
        nino.alias = nuevo;
        nino.nombre = nombre;
        if (avatar) nino.avatar = avatar;
        for (const sesion of this.datos.sesiones) if (sesion.alias === viejo) sesion.alias = nuevo;
        const abierta = this.datos.sesion?.alias === viejo;
        if (abierta) this.datos.sesion.alias = nuevo;
        this.guardar();
        return resp(200, { alias: nuevo }, abierta ? [this.mensajeSesion()] : []);
      }
      case 'POST ninos/borrar': {
        const nino = this.nino(cuerpo.alias);
        if (!nino) return error(404, 'ese niño ya no está en la lista');
        if (this.datos.sesion?.alias === nino.alias) return error(409, 'está en sesión: termínala antes de borrarlo');
        this.datos.ninos = this.datos.ninos.filter((n) => n !== nino);
        this.datos.sesiones = this.datos.sesiones.filter((x) => x.alias !== nino.alias);
        this.guardar();
        return resp(204);
      }
      case 'GET historial':
        return resp(200, this.historial(cuerpo.alias, cuerpo.periodo));
      default:
        return error(404, 'no existe');
    }
  }

  nino(alias) {
    const buscado = normalizarAlias(alias);
    return this.datos.ninos.find((n) => normalizarAlias(n.alias) === buscado && buscado !== null);
  }

  // Para quien solo escribe el nombre: N-01, N-02…
  aliasLibre() {
    let i = 1;
    while (this.nino(`N-${i}`)) i += 1;
    return normalizarAlias(`N-${i}`);
  }

  sesionesDe(alias) {
    return this.datos.sesiones.filter((s) => s.alias === alias);
  }

  reparto(sesiones) {
    const total = {};
    for (const s of sesiones) {
      for (const n of ANIMALES) {
        if (s.resumen.gestos[n]) total[n] = (total[n] ?? 0) + s.resumen.gestos[n];
      }
    }
    return total;
  }

  fichaNino(alias) {
    const sesiones = this.sesionesDe(alias);
    const turnos = sesiones.reduce((t, s) => t + s.resumen.simon.turnos, 0);
    const aciertos = sesiones.reduce((t, s) => t + s.resumen.simon.aciertos, 0);
    const favorito = Object.entries(this.reparto(sesiones)).sort((a, b) => b[1] - a[1])[0];
    return {
      alias, nombre: this.nino(alias)?.nombre ?? '', sesiones: sesiones.length, simon_pct: turnos ? Math.round((aciertos * 100) / turnos) : null,
      favorito: favorito ? favorito[0] : null, avatar: this.nino(alias)?.avatar ?? null, en_sesion: this.datos.sesion?.alias === alias,
    };
  }

  historial(alias, periodo) {
    const desde = this.ahora() - (DIAS[periodo] ?? DIAS.mes) * 86400000;
    const sesiones = this.sesionesDe(alias).filter((s) => s.inicio >= desde).sort((a, b) => b.inicio - a.inicio);
    return {
      sesiones: sesiones.map((s) => ({
        fecha: fechaLocal(s.inicio), modos: s.modos, minutos: Math.round((s.fin - s.inicio) / 60000), cancion: s.cancion,
        gestos: ANIMALES.reduce((t, n) => t + s.resumen.gestos[n], 0),
        simon: s.resumen.simon.turnos ? [s.resumen.simon.aciertos, s.resumen.simon.turnos] : null, aviso: s.aviso, nota: s.nota || '',
      })),
      reparto: this.reparto(sesiones),
    };
  }
}

// AAAA-MM-DD en la hora de la laptop (toISOString daría el día en UTC).
function fechaLocal(ms) {
  const d = new Date(ms);
  return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
}

module.exports = { Orquesta, descifrar };
