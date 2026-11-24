// Puente entre el broker y la interfaz web: descifra los eventos del osito, confirma los que ya
// se guardaron, lleva la sesión y arma los mensajes que esperan las pantallas.
// No depende de Node-RED: los flujos solo lo llaman (así se prueba con `node --test`).
'use strict';

const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

const NODOS = ['osito', 'ranita', 'gatito', 'pollito'];
const ANIMALES = ['gatito', 'ranita', 'pollito'];
const MODOS = ['cancion', 'libre', 'simon'];
// Solo lo que el osito tiene transcrito en canciones.h; cualquier otra la rechaza.
const CANCIONES = [{ nombre: 'prueba', tipo: 'alegres', duracion: '0:04' }];
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
  constructor({ claveHex, archivo, ahora = Date.now, aviso = () => {} }) {
    if (!/^[0-9a-fA-F]{32}$/.test(claveHex || '')) {
      throw new Error('MQTT_AES_KEY_HEX debe tener 32 caracteres hex');
    }
    this.clave = Buffer.from(claveHex, 'hex');
    this.archivo = archivo;
    this.ahora = ahora;
    this.aviso = aviso;
    // Lo que se conserva al reiniciar Node-RED.
    this.datos = {
      ninos: [], sesiones: [], sesion: null, siguienteId: 1, avisoSilenciado: false,
      modo: 'cancion', cancion: CANCIONES[0].nombre, sync: { arranque: null, hasta: 0, recibidos: [] },
    };
    this.conectado = {};
    this.ultimoGesto = {};
    this.sombra = 0;
    this.volumen = null;
    this.ositoEnLinea = null;
    this.pendientes = 0;
    this.volvioEn = null; // reconectado, falta que diga cuántos eventos traía
    this.huecoDesde = null;
    this.pedidos = new Map(); // id -> canción pedida
    this.guardado = null;
    this.cargar();
  }

  cargar() {
    if (!this.archivo || !fs.existsSync(this.archivo)) return;
    // Si el archivo está dañado se avisa y no se arranca: escribir encima perdería el historial.
    Object.assign(this.datos, JSON.parse(fs.readFileSync(this.archivo, 'utf8')));
  }

  guardar() {
    if (this.archivo) {
      fs.mkdirSync(path.dirname(this.archivo), { recursive: true });
      const temporal = `${this.archivo}.tmp`;
      fs.writeFileSync(temporal, JSON.stringify(this.datos), { mode: 0o600 });
      fs.renameSync(temporal, this.archivo); // nunca queda a medias
    }
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
    else if (!NODOS.includes(nodo) || tercero) return salida;
    else if (cual === 'estado') this.alEstado(nodo, datos, salida);
    else if (cual === 'evento') this.alSobre(nodo, datos, salida);
    else if (topic === 'orquesta/osito/volumen') this.alVolumen(datos, salida);
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
    this.conectado[nodo] = conectado;
    salida.ws.push({ tipo: 'estado', nodo, conectado: this.estadoDe(nodo) });
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
    const msg = { tipo: 'red', internet: this.ositoEnLinea !== false && this.volvioEn === null, pendientes: this.pendientes };
    if (volvio) msg.recuperados = this.pendientes;
    if (this.guardado) msg.guardado = this.guardado;
    return msg;
  }

  alAckModo(datos, salida) {
    if (!MODOS.includes(datos.modo)) return;
    if (datos.ok) {
      this.datos.modo = datos.modo;
      this.datos.cancion = this.pedidos.get(datos.id) ?? this.datos.cancion;
      this.anotarModo(datos.modo);
      this.guardar();
    }
    this.pedidos.delete(datos.id);
    salida.ws.push({ tipo: 'modo', id: datos.id, ok: Boolean(datos.ok), modo: this.datos.modo, cancion: this.datos.cancion });
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
        this.anotarModo(evento.modo);
        // El que pidió Node-RED ya llegó por el acuse, con su id.
        if (evento.origen !== 'nodered') {
          salida.ws.push({ tipo: 'modo', id: null, ok: true, modo: evento.modo, cancion: evento.cancion });
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
      if (nodo in this.conectado) ws.push({ tipo: 'estado', nodo, conectado: this.estadoDe(nodo) });
    }
    ws.push(this.red());
    if (this.volumen !== null) ws.push({ tipo: 'volumen', nodo: 'osito', valor: this.volumen });
    return ws;
  }

  mensajeSesion() {
    const s = this.datos.sesion;
    return {
      tipo: 'sesion', alias: s?.alias ?? null, inicio: s?.inicio ?? null, modo: this.datos.modo,
      cancion: this.datos.cancion, resumen: s?.resumen ?? resumenVacio(), silenciado: this.datos.avisoSilenciado,
    };
  }

  anotarModo(modo) {
    const s = this.datos.sesion;
    if (s && !s.modos.includes(modo)) s.modos.push(modo);
  }

  // { codigo, cuerpo, ws, mqtt }. El MQTT se publica después de responder: así la pantalla ya
  // conoce el id cuando llega la confirmación del osito.
  alHttp(metodo, ruta, datos = {}) {
    const resp = (codigo, cuerpo = null, ws = [], mqtt = []) => ({ codigo, cuerpo, ws, mqtt });
    const error = (codigo, texto) => resp(codigo, { error: texto });
    const cuerpo = datos && typeof datos === 'object' ? datos : {};
    switch (`${metodo} ${ruta}`) {
      case 'POST modo': {
        if (!MODOS.includes(cuerpo.modo)) return error(400, 'modo desconocido');
        const pedido = { id: this.datos.siguienteId, modo: cuerpo.modo };
        if (cuerpo.cancion) {
          if (!CANCIONES.some((c) => c.nombre === cuerpo.cancion)) return error(400, 'el osito no tiene esa canción');
          pedido.cancion = cuerpo.cancion;
        }
        if (!this.ositoEnLinea) return error(503, 'el osito no está en línea');
        this.datos.siguienteId += 1;
        this.pedidos.set(pedido.id, pedido.cancion);
        this.guardar();
        return resp(200, { id: pedido.id }, [], [{ topic: 'orquesta/control/modo', payload: JSON.stringify(pedido) }]);
      }
      case 'POST aviso':
        if (typeof cuerpo.silenciar !== 'boolean') return error(400, 'falta silenciar');
        if (!this.ositoEnLinea) return error(503, 'el osito no está en línea');
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
          alias: s.alias, inicio: s.inicio, fin: this.ahora(), modos: s.modos, cancion: this.datos.cancion,
          resumen: s.resumen, aviso: s.aviso, nota: String(cuerpo.nota ?? '').slice(0, 2000),
        });
        this.datos.sesion = null;
        this.guardar();
        return resp(204, null, [this.mensajeSesion()]);
      }
      case 'GET canciones':
        return resp(200, CANCIONES);
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
        this.datos.ninos.push({ alias, nombre });
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
        const viejo = nino.alias;
        nino.alias = nuevo;
        nino.nombre = nombre;
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
      favorito: favorito ? favorito[0] : null, en_sesion: this.datos.sesion?.alias === alias,
    };
  }

  historial(alias, periodo) {
    const desde = this.ahora() - (DIAS[periodo] ?? DIAS.mes) * 86400000;
    const sesiones = this.sesionesDe(alias).filter((s) => s.inicio >= desde).sort((a, b) => b.inicio - a.inicio);
    return {
      sesiones: sesiones.map((s) => ({
        fecha: fechaLocal(s.inicio), modos: s.modos, minutos: Math.round((s.fin - s.inicio) / 60000), cancion: s.cancion,
        gestos: ANIMALES.reduce((t, n) => t + s.resumen.gestos[n], 0),
        simon: s.resumen.simon.turnos ? [s.resumen.simon.aciertos, s.resumen.simon.turnos] : null, aviso: s.aviso,
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
