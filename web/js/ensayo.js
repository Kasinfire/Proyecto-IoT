// Modo ensayo: simula a Node-RED para probar la interfaz sin osito ni broker.
// Se activa abriendo cualquier página con ?ensayo (por ejemplo en-vivo.html?ensayo).
// Los mensajes que genera tienen el mismo formato que los reales (ver nodered/README.md).

const MELODIA = ['do', 'do', 'sol', 'sol', 'la', 'la', 'sol', 'fa', 'fa', 'mi', 'mi', 're', 're', 'do'];
const MINUTO = 60 * 1000;

export const CANCIONES = [
  { nombre: 'Estrellita', tipo: 'tranquilas', duracion: '1:20' },
  { nombre: 'Arrorró mi niño', tipo: 'tranquilas', duracion: '1:30' },
  { nombre: 'Duérmete mi niño', tipo: 'tranquilas', duracion: '1:25' },
  { nombre: 'Los pollitos dicen', tipo: 'alegres', duracion: '1:05' },
  { nombre: 'La araña pequeñita', tipo: 'alegres', duracion: '1:10' },
  { nombre: 'Pin pon', tipo: 'alegres', duracion: '1:15' },
  { nombre: 'Un elefante se columpiaba', tipo: 'alegres', duracion: '1:30' },
  { nombre: 'Noche de paz', tipo: 'navidad', duracion: '1:50' },
  { nombre: 'Campana sobre campana', tipo: 'navidad', duracion: '1:20' },
  { nombre: 'Los peces en el río', tipo: 'navidad', duracion: '1:35' },
  { nombre: 'Cascabel', tipo: 'navidad', duracion: '1:10' },
  { nombre: 'Arre borriquito', tipo: 'navidad', duracion: '1:00' },
  { nombre: 'Las mañanitas', tipo: 'cumpleanos', duracion: '1:40' },
  { nombre: 'Cumpleaños feliz', tipo: 'cumpleanos', duracion: '0:40' },
];

export const NINOS = [
  { alias: 'A-07', sesiones: 6, simon_pct: 75, favorito: 'gatito', en_sesion: true },
  { alias: 'B-12', sesiones: 4, simon_pct: 60, favorito: 'ranita', en_sesion: false },
  { alias: 'C-03', sesiones: 2, simon_pct: null, favorito: 'pollito', en_sesion: false },
];

const HISTORIAL_A07 = {
  sesiones: [
    { fecha: '2026-11-20', modos: ['cancion', 'simon'], minutos: 18, cancion: 'Estrellita', gestos: 299, simon: [9, 12], aviso: 'Pollito se desconectó' },
    { fecha: '2026-11-16', modos: ['cancion', 'libre', 'simon'], minutos: 16, cancion: 'Estrellita', gestos: 264, simon: [7, 10], aviso: null },
    { fecha: '2026-11-13', modos: ['cancion', 'simon'], minutos: 17, cancion: 'Los pollitos dicen', gestos: 251, simon: [8, 13], aviso: null },
    { fecha: '2026-11-09', modos: ['libre', 'simon'], minutos: 15, cancion: 'Los pollitos dicen', gestos: 238, simon: [6, 11], aviso: 'Sin internet 3 min' },
    { fecha: '2026-11-06', modos: ['cancion'], minutos: 17, cancion: 'Estrellita', gestos: 270, simon: null, aviso: null },
    { fecha: '2026-11-02', modos: ['cancion', 'libre'], minutos: 14, cancion: 'Estrellita', gestos: 212, simon: [4, 10], aviso: null },
  ],
  reparto: { gatito: 742, ranita: 510, pollito: 288 },
  nota: { texto: 'Evita al pollito; le cuesta sacudirlo. Probar sentado y con más tiempo.', fecha: '2026-11-20' },
};

export function historialDe(alias) {
  if (alias === 'A-07') return HISTORIAL_A07;
  return { sesiones: [], reparto: {}, nota: null };
}

export function sesionActual() {
  return { alias: 'A-07', inicio: Date.now() - (18 * MINUTO + 42 * 1000) };
}

// Arranca la simulación y devuelve funciones para responder a lo que pide la interfaz.
export function iniciarSimulacion(emitir) {
  const conectados = { osito: true, ranita: true, gatito: true, pollito: true };
  let modo = 'cancion';
  let cancion = 'Estrellita';
  let siguienteId = 15;
  let sinInternet = false;
  let pendientes = 0;
  let paso = 0;

  // Si no hay internet, el osito "guarda" los eventos en vez de mandarlos.
  const enviar = (msg) => {
    if (sinInternet && msg.nodo) {
      pendientes += 1;
      emitir({ tipo: 'red', internet: false, pendientes });
    }
    emitir(msg);
  };

  const inicio = sesionActual();
  emitir({
    tipo: 'sesion', alias: inicio.alias, inicio: inicio.inicio, modo, cancion,
    resumen: { gestos: { gatito: 142, ranita: 96, pollito: 61 }, simon: { aciertos: 9, turnos: 12 }, juntos_s: 250 },
  });
  Object.keys(conectados).forEach((nodo) => emitir({ tipo: 'estado', nodo, conectado: true }));
  emitir({ tipo: 'red', internet: true, pendientes: 0, guardado: Date.now() });
  emitir({ tipo: 'volumen', nodo: 'osito', valor: 70 });

  // A los 6 s el pollito "se apaga", para ver la alerta.
  setTimeout(() => {
    conectados.pollito = false;
    emitir({ tipo: 'estado', nodo: 'pollito', conectado: false });
  }, 6000);

  setInterval(() => {
    paso += 1;
    // Ranita: tapa la flor a ratos
    const tapando = paso % 40 < 22;
    if (conectados.ranita && paso % 2 === 0) {
      enviar({ tipo: 'sombra', nodo: 'ranita', nivel: tapando ? 40 + (paso % 5) * 5 : 0 });
    }
    // Gatito: toca la melodía con pausas
    if (conectados.gatito && paso % 4 === 0 && paso % 60 < 36) {
      enviar({ tipo: 'nota', nodo: 'gatito', nota: MELODIA[(paso / 4) % MELODIA.length], vel: 90 });
    }
    // Pollito: sacude de vez en cuando mientras está conectado
    if (conectados.pollito && paso % 9 === 0) {
      enviar({ tipo: 'agitar', nodo: 'pollito', intensidad: 60 + (paso % 4) * 10 });
    }
    if (paso % 4 === 0) {
      emitir({ tipo: 'cancion', progreso: ((paso / 4) % 80) / 80 });
    }
  }, 250);

  // Simón Dice: el osito pide un animal y luego llega el resultado.
  setInterval(() => {
    if (modo !== 'simon') return;
    const candidatos = ['ranita', 'gatito', 'pollito'].filter((n) => conectados[n]);
    if (!candidatos.length) return;
    const nodo = candidatos[Math.floor(Math.random() * candidatos.length)];
    emitir({ tipo: 'simon', nodo });
    setTimeout(() => emitir({ tipo: 'simon', nodo, resultado: Math.random() < 0.75 ? 'acierto' : 'error' }), 1800);
  }, 4000);

  return {
    pedirModo(nuevoModo, nuevaCancion) {
      const id = siguienteId++;
      setTimeout(() => {
        modo = nuevoModo;
        cancion = nuevaCancion || cancion;
        emitir({ tipo: 'modo', id, ok: true, modo, cancion });
      }, 700);
      return { id };
    },
    simularCaida(caida) {
      sinInternet = caida;
      if (caida) {
        pendientes = 0;
        emitir({ tipo: 'red', internet: false, pendientes });
      } else {
        emitir({ tipo: 'red', internet: true, pendientes: 0, recuperados: pendientes, guardado: Date.now() });
      }
    },
    reconectar(nodo) {
      conectados[nodo] = true;
      emitir({ tipo: 'estado', nodo, conectado: true });
    },
  };
}
