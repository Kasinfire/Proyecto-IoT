// Toca una canción en el navegador (WebAudio) para oírla antes de guardarla. Es una muestra
// sencilla: el sonido de verdad sale por el sintetizador de la laptop.

const VOLUMEN = 0.16;
const PARTES = { osito: ['triangle', 0.9], ranita: ['sine', 0.5], gatito: ['triangle', 1], pollito: ['ruido', 0.5] };

let contexto = null;
let sonando = null;

const hz = (nota) => 440 * 2 ** ((nota + 12 - 69) / 12);

function ruido(ctx) {
  const datos = ctx.createBuffer(1, ctx.sampleRate * 0.08, ctx.sampleRate);
  const canal = datos.getChannelData(0);
  for (let i = 0; i < canal.length; i += 1) canal[i] = Math.random() * 2 - 1;
  return datos;
}

export function parar() {
  if (!sonando) return;
  clearTimeout(sonando.reloj);
  sonando.salida.gain.setTargetAtTime(0, contexto.currentTime, 0.02);
  for (const f of sonando.fuentes) f.stop(contexto.currentTime + 0.1);
  const { alTerminar } = sonando;
  sonando = null;
  alTerminar();
}

// partes: { osito: [[nota, pulsos, fuerza?], …], … }
export async function tocar(partes, pulsoMs, alTerminar) {
  parar();
  contexto ??= new AudioContext();
  await contexto.resume();
  const salida = contexto.createGain();
  salida.gain.value = VOLUMEN;
  salida.connect(contexto.destination);
  const fuentes = [];
  const inicio = contexto.currentTime + 0.08;
  const total = (partes.gatito ?? []).reduce((t, [, d]) => t + d, 0);
  const golpe = ruido(contexto);
  for (const [parte, pasos] of Object.entries(partes)) {
    const [tipo, fuerza] = PARTES[parte];
    if (!pasos.length) continue;
    for (let t = 0, i = 0; t < total; i += 1) {
      const [nota, dura, vel = 90] = pasos[i % pasos.length];
      if (nota) {
        const cuando = inicio + (t * pulsoMs) / 1000;
        const fin = cuando + (Math.min(dura, total - t) * pulsoMs * 0.95) / 1000;
        const nivel = contexto.createGain();
        nivel.gain.setValueAtTime(0, cuando);
        nivel.gain.linearRampToValueAtTime((fuerza * vel) / 100, cuando + 0.01);
        nivel.gain.setTargetAtTime(0, tipo === 'ruido' ? cuando + 0.02 : fin - 0.04, 0.03);
        nivel.connect(salida);
        const fuente = tipo === 'ruido' ? contexto.createBufferSource() : contexto.createOscillator();
        if (tipo === 'ruido') fuente.buffer = golpe;
        else { fuente.type = tipo; fuente.frequency.value = hz(nota); }
        fuente.connect(nivel);
        fuente.start(cuando);
        fuente.stop(tipo === 'ruido' ? cuando + 0.08 : fin + 0.15);
        fuentes.push(fuente);
      }
      t += dura;
    }
  }
  sonando = { salida, fuentes, alTerminar, reloj: setTimeout(parar, (total * pulsoMs) + 400) };
}

export function nota(midi) {
  tocar({ gatito: [[midi, 2]] }, 200, () => {});
}
