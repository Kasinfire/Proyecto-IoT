// Dónde se guardan niños, sesiones y canciones: un archivo en esta laptop, que es la fuente de verdad.
// A Supabase (nube.js) solo sube lo que da paraNube(): nunca el nombre del niño ni la nota.
'use strict';

const fs = require('node:fs');
const path = require('node:path');

class AlmacenArchivo {
  constructor(archivo) {
    this.archivo = archivo;
  }

  // null si todavía no hay nada. Si el archivo está dañado lanza un error: escribir encima
  // perdería el historial.
  leer() {
    if (!this.archivo || !fs.existsSync(this.archivo)) return null;
    return JSON.parse(fs.readFileSync(this.archivo, 'utf8'));
  }

  guardar(datos) {
    if (!this.archivo) return;
    fs.mkdirSync(path.dirname(this.archivo), { recursive: true });
    const temporal = `${this.archivo}.tmp`;
    fs.writeFileSync(temporal, JSON.stringify(datos), { mode: 0o600 });
    fs.renameSync(temporal, this.archivo); // nunca queda a medias
  }
}

function paraNube({ ninos, sesiones, canciones }) {
  return {
    ninos: ninos.map(({ alias }) => ({ alias })),
    sesiones: sesiones.map(({ nota, ...resto }) => resto), // la nota puede traer datos personales
    canciones,
  };
}

module.exports = { AlmacenArchivo, paraNube };
