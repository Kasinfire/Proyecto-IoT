// Copia en Supabase (nube) de los niños y las sesiones. Habla con la API REST de Supabase
// (PostgREST) y cada petición espera su código HTTP: es Request/Response.
// Solo recibe lo que ya pasó por paraNube(): nunca el nombre del niño ni la nota de la sesión.
'use strict';

// Resultado de enviar(): ok, reintentar (red caída, Supabase pausado, llave mal, tablas sin crear) o
// descartar (Supabase dice que el dato no es válido: repetirlo no lo arregla; orquesta.js lo aparta).
const OK = 'ok';
const REINTENTAR = 'reintentar';
const DESCARTAR = 'descartar';

class NubeSupabase {
  constructor({ url, llave, fetch = globalThis.fetch, esperaMs = 15000 }) {
    if (!url || !llave) throw new Error('Faltan SUPABASE_URL o SUPABASE_KEY');
    this.base = `${String(url).replace(/\/+$/, '')}/rest/v1`;
    this.fetch = fetch;
    this.esperaMs = esperaMs;
    this.cabeceras = { apikey: llave, 'Content-Type': 'application/json', Prefer: 'return=minimal' };
    // Las llaves viejas (JWT service_role) también van en Authorization; las nuevas (sb_secret_…) no.
    if (!llave.startsWith('sb_')) this.cabeceras.Authorization = `Bearer ${llave}`;
  }

  // op: { op: 'nino', alias, avatar } | { op: 'renombrar', de, a } | { op: 'borrarNino', alias }
  //   | { op: 'sesion', sesion: { id, alias, inicio, fin, modos, cancion, resumen, aviso } }
  // Devuelve { resultado, detalle }.
  async enviar(op) {
    const alias = (a) => `alias=eq.${encodeURIComponent(a)}`;
    switch (op.op) {
      case 'nino':
        return this.pedir('POST', 'ninos?on_conflict=alias', [{ alias: op.alias, avatar: op.avatar ?? null }], true);
      case 'renombrar':
        return this.pedir('PATCH', `ninos?${alias(op.de)}`, { alias: op.a });
      case 'borrarNino':
        return this.pedir('DELETE', `ninos?${alias(op.alias)}`);
      case 'sesion': {
        const s = op.sesion;
        const fila = {
          id: s.id, alias: s.alias, inicio: new Date(s.inicio).toISOString(), fin: new Date(s.fin).toISOString(),
          modos: s.modos, cancion: s.cancion, resumen: s.resumen, aviso: s.aviso ?? null,
        };
        const subir = () => this.pedir('POST', 'sesiones?on_conflict=id', [fila], true);
        const primero = await subir();
        if (primero.estado !== 409) return primero;
        // Falta su niño en la nube: se crea (sin tocar su avatar si ya está) y se intenta otra vez.
        const nino = await this.pedir('POST', 'ninos?on_conflict=alias', [{ alias: s.alias }], true, 'ignore-duplicates');
        return nino.resultado === OK ? subir() : nino;
      }
      default:
        return { resultado: DESCARTAR, detalle: `operación desconocida: ${op.op}` };
    }
  }

  async pedir(metodo, ruta, cuerpo, upsert = false, siExiste = 'merge-duplicates') {
    const headers = { ...this.cabeceras };
    // Repetir una operación (si se cortó la red tras guardar) no duplica nada.
    if (upsert) headers.Prefer = `resolution=${siExiste},return=minimal`;
    let respuesta;
    try {
      respuesta = await this.fetch(`${this.base}/${ruta}`, {
        method: metodo, headers, body: cuerpo === undefined ? undefined : JSON.stringify(cuerpo),
        signal: AbortSignal.timeout(this.esperaMs),
      });
    } catch (error) {
      return { resultado: REINTENTAR, detalle: `sin conexión con Supabase (${error.message})` };
    }
    if (respuesta.ok) return { resultado: OK };
    const texto = await respuesta.text().catch(() => '');
    const detalle = `Supabase respondió ${respuesta.status}: ${texto.slice(0, 200)}`;
    // 401/403: la llave está mal o falta una política; 404: tablas sin crear o URL mal; 408/429/5xx:
    // pasajero. Todos se arreglan sin cambiar el dato, así que se reintentan.
    const pasajero = [401, 403, 404, 408, 429].includes(respuesta.status) || respuesta.status >= 500;
    return { resultado: pasajero ? REINTENTAR : DESCARTAR, detalle, estado: respuesta.status };
  }
}

module.exports = { NubeSupabase, OK, REINTENTAR, DESCARTAR };
