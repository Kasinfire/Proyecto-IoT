// Escribe firmware/osito/canciones_datos.h a partir de nodered/canciones.json, para que las
// canciones incluidas suenen aunque no haya red. Con --revisar solo dice si está al día.
import fs from 'node:fs';
import { createRequire } from 'node:module';

const { PARTES, arreglar, incluidas } = createRequire(import.meta.url)('../nodered/arreglo.js');
const DESTINO = new URL('../firmware/osito/canciones_datos.h', import.meta.url);

const lineas = [
  '// GENERADO por herramientas/generar_canciones.mjs desde nodered/canciones.json: no editar a mano.',
  '// Tradicionales o de dominio público, y piezas originales ("bloques-...").',
  '#pragma once', '', '#include "cancion.h"', '', 'namespace canciones {',
];
const tabla = [];
for (const c of incluidas()) {
  const partes = arreglar(c);
  const ids = PARTES.map((p) => {
    const id = `${c.id.replaceAll('-', '_')}_${p}`.toUpperCase();
    const pasos = partes[p].map((x) => `{${x.join(', ')}}`);
    lineas.push(`inline constexpr Paso ${id}[] = {`);
    for (let i = 0; i < pasos.length; i += 8) lineas.push(`    ${pasos.slice(i, i + 8).join(', ')},`);
    lineas.push('};');
    return `{${id}, std::size(${id})}`;
  });
  lineas.push('');
  tabla.push(`    {"${c.id}", ${c.pulso_ms}, {${ids.slice(0, 2).join(', ')},`, `     ${ids.slice(2).join(', ')}}},`);
}
lineas.push('inline constexpr Cancion INCLUIDAS[] = {', ...tabla, '};', '}  // namespace canciones', '');
const texto = lineas.join('\n');
if (process.argv.includes('--revisar')) {
  const igual = fs.existsSync(DESTINO) && fs.readFileSync(DESTINO, 'utf8') === texto;
  console.log(igual ? 'canciones_datos.h está al día' : 'canciones_datos.h NO está al día: corre generar_canciones.mjs');
  process.exit(igual ? 0 : 1);
}
fs.writeFileSync(DESTINO, texto);
console.log(`${tabla.length / 2} canciones escritas`);
