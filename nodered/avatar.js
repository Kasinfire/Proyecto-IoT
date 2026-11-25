// Avatar de un niño: un animal vestido. Solo se guardan ids de estas listas (nunca imágenes ni texto
// libre). El dibujo está en web/js/avatar.js; una prueba revisa que las dos listas coincidan.
'use strict';

const COLORES = ['rojo', 'naranja', 'amarillo', 'verde', 'azul', 'morado', 'rosa', 'negro'];
const PARTES = {
  animal: ['osito', 'ranita', 'gatito', 'pollito'],
  gorro: ['ninguno', 'gorra', 'lana', 'corona', 'mago', 'mono', 'flor', 'audifonos'],
  lentes: ['ninguno', 'redondos', 'sol', 'corazones'],
  ropa: ['ninguna', 'camiseta', 'rayas', 'overol', 'vestido', 'sudadera', 'capa'],
  accesorio: ['ninguno', 'bufanda', 'corbatin', 'collar', 'medalla', 'globo'],
  color_gorro: COLORES,
  color_ropa: COLORES,
  fondo: ['durazno', 'menta', 'lavanda', 'limon', 'rosa', 'lima'],
};

// El avatar con solo sus campos, o null si algo no está en las listas.
function leerAvatar(datos) {
  if (!datos || typeof datos !== 'object') return null;
  const limpio = {};
  for (const [parte, ids] of Object.entries(PARTES)) {
    if (!Object.hasOwn(datos, parte) || !ids.includes(datos[parte])) return null;
    limpio[parte] = datos[parte];
  }
  return limpio;
}

module.exports = { PARTES, leerAvatar };
