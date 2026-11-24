"""Toca en la laptop las notas que el osito imprime por serie.

Lee "[AUDIO-REGISTRO] ON canal nota velocidad", "OFF canal nota", "EXP canal valor" y "VOL valor".
Solo lee el puerto: no le manda nada a la placa. Las demás líneas las muestra tal cual.

  python3 herramientas/sintetizador.py /dev/cu.usbserial-110
  python3 herramientas/sintetizador.py /dev/cu.usbserial-110 --medir   (hora de cada línea)

--medir pone los ms desde el inicio en cada [RADIO], [simon], [dj] y nota. Son la hora en que la
línea llegó a la laptop; a eso se suma la latencia de audio que se imprime al abrir.
Necesita: python3 -m pip install pyserial numpy sounddevice
"""
import argparse
import re
import threading
import time

import numpy as np
import serial
import sounddevice as sd

TASA = 44100
TRANSPONER = 12  # una octava arriba: los graves casi no se oyen en bocinas chicas
SIN_OFF_S = 0.4  # si el firmware no imprime OFF, la nota se apaga sola

PATRON = re.compile(r"\[AUDIO-(?:DAC|REGISTRO)\] (ON|OFF|EXP|VOL) ([\d ]+)")

candado = threading.Lock()
voces = {}  # (canal, nota) -> dict
expresion = {c: 127 for c in range(1, 17)}
volumen = [64]
vio_off = [False]


def hz(nota):
    return 440.0 * 2 ** ((min(nota + TRANSPONER, 127) - 69) / 12)


# Piano: armónicos (fuerza, qué tan rápido se apagan) y un ataque corto.
ARMONICOS = [(1.0, 1.0), (0.5, 1.6), (0.3, 2.3), (0.15, 3.0), (0.08, 4.0)]
CAIDA_PIANO_S = 1.2
SOLTAR_S = 0.12  # al soltar la tecla (OFF), se apaga en este tiempo
ATAQUE_S = 0.004


# Percusión (canal 10, notas General MIDI): (cuánto se apaga en s, duración total en s).
PERCUSION = {
    36: (0.12, 0.4),    # bombo (ritmo de fondo)
    37: (0.015, 0.08),  # clic del metrónomo del looper
    38: (0.08, 0.3),    # tambor (redoble del DJ)
    42: (0.025, 0.15),  # platillo cerrado: "ts"
    46: (0.25, 0.9),    # platillo abierto: "tssss"
    49: (0.6, 2.0),     # platillo de choque (final del redoble y del "drop")
    54: (0.12, 0.5),    # pandereta
    56: (0.12, 0.5),    # cencerro
    70: (0.04, 0.15),   # maraca: "chk"
    76: (0.035, 0.2),   # wood block agudo
    77: (0.035, 0.2),   # wood block grave
    90: (1.0, 0.24),    # pío-pío del pollito (fuera de General MIDI, igual que el osito)
}
NOTA_PIO = 90
PERCUSION_OTRA = (0.03, 0.2)


def pio(t):
    # Dos píos: cada uno sube rápido de tono, como un pollito.
    onda = np.zeros_like(t)
    for inicio in (0.0, 0.12):
        dentro = (t >= inicio) & (t < inicio + 0.08)
        tl = np.where(dentro, t - inicio, 0.0)
        # De 1800 a 3200 Hz en 80 ms: fase = integral de la frecuencia.
        fase = 2 * np.pi * (1800 * tl + (3200 - 1800) / (2 * 0.08) * tl ** 2)
        env = np.sin(np.pi * tl / 0.08)  # entra y sale suave
        onda += np.where(dentro, np.sin(fase) * env, 0.0)
    return onda * 0.7



def ruido_agudo(n):
    # Ruido blanco menos su versión anterior: quita graves y suena más a "ts".
    r = np.random.uniform(-1, 1, n + 1)
    return (r[1:] - r[:-1]) / 2


def percusion(nota, t, n):
    if nota == NOTA_PIO:
        return pio(t)
    caida, _ = PERCUSION.get(nota, PERCUSION_OTRA)
    env = np.exp(-t / caida)
    if nota in (42, 46):
        return ruido_agudo(n) * env
    if nota == 36:  # bombo: tono grave que baja rápido
        fase = 2 * np.pi * (55 * t + 90 * 0.03 * (1 - np.exp(-t / 0.03)))
        return np.sin(fase) * env * 1.3
    if nota == 38:  # tambor: golpe grave + cuerda de ruido
        return (np.sin(2 * np.pi * 180 * t) * 0.6 + np.random.uniform(-1, 1, n) * 0.5) * env
    if nota == 49:  # choque: ruido brillante con brillo metálico, largo
        brillo = sum(np.sin(2 * np.pi * f * t) for f in (3200, 4700, 6100)) / 3
        return (ruido_agudo(n) * 0.8 + brillo * 0.3) * env
    if nota == 70:
        ataque = np.minimum(t / 0.006, 1.0)  # la maraca "arrastra" un poco al inicio
        return ruido_agudo(n) * env * ataque * 0.8
    if nota == 54:
        sonajas = sum(np.sin(2 * np.pi * f * t) for f in (5300, 6900, 8100)) / 3
        return (ruido_agudo(n) * 0.6 + sonajas * 0.5) * env
    if nota == 56:
        cuadrada = lambda f: np.sign(np.sin(2 * np.pi * f * t))  # noqa: E731
        return (cuadrada(540) + cuadrada(800)) * 0.3 * env
    if nota in (76, 77):
        return np.sin(2 * np.pi * (1100 if nota == 76 else 800) * t) * env
    return np.random.uniform(-1, 1, n) * env


# Violín (ranita, canal 2): arco que entra despacio, nota sostenida y vibrato que aparece
# después de empezar, como lo hace un violinista.
CANAL_VIOLIN = 2
CANAL_COLCHON = 14  # acordes de fondo
COLCHON_ATAQUE_S = 0.15
COLCHON_SOLTAR_S = 0.3
CANALES_PERCUSION = (10, 16)  # 10 = pollito, 16 = efectos del DJ
VIOLIN_ARMONICOS = 10
VIOLIN_ATAQUE_S = 0.025  # más lento se siente atrasado en Simón Dice
VIBRATO_HZ = 5.5
VIBRATO_PROFUNDIDAD = 0.006  # ±0.6 % de la frecuencia
VIBRATO_DESDE_S = 0.25
VIOLIN_SOLTAR_S = 0.18


def violin(voz, t):
    f = hz(voz["nota"])
    cuanto_vibrato = np.clip((t - VIBRATO_DESDE_S) / 0.3, 0.0, 1.0) * VIBRATO_PROFUNDIDAD
    # Fase con vibrato: integral de f * (1 + d * sin(2π v t)).
    fase = 2 * np.pi * f * (
        t - cuanto_vibrato / (2 * np.pi * VIBRATO_HZ) * np.cos(2 * np.pi * VIBRATO_HZ * t)
    )
    onda = np.zeros_like(t)
    for k in range(1, VIOLIN_ARMONICOS + 1):
        if f * k > TASA / 2:
            break
        # Como una onda de sierra (todos los armónicos), un poco más suave en los agudos.
        onda += np.sin(k * fase) / k ** 1.2
    onda *= np.minimum(t / VIOLIN_ATAQUE_S, 1.0)
    if voz.get("soltada") is not None:
        onda *= np.exp(-np.maximum(t - voz["soltada"], 0) / VIOLIN_SOLTAR_S)
    return onda * 0.45


def colchon(voz, t):
    # Acorde suave y sostenido: pocos armónicos, entra y sale despacio.
    f = hz(voz["nota"])
    onda = sum(np.sin(2 * np.pi * f * k * t) * p for k, p in ((1, 1.0), (2, 0.25), (3, 0.08)))
    onda = onda * np.minimum(t / COLCHON_ATAQUE_S, 1.0)
    if voz.get("soltada") is not None:
        onda = onda * np.exp(-np.maximum(t - voz["soltada"], 0) / COLCHON_SOLTAR_S)
    return onda * 0.4


def generar(voz, n):
    muestras = voz["fase"] + np.arange(n)
    t = muestras / TASA
    voz["fase"] += n
    if voz["canal"] in CANALES_PERCUSION:  # cada nota es un instrumento distinto
        return percusion(voz["nota"], t, n).astype(np.float32)
    if voz["canal"] == CANAL_COLCHON:
        return colchon(voz, t).astype(np.float32)
    if voz["canal"] == CANAL_VIOLIN:
        return violin(voz, t).astype(np.float32)
    f = hz(voz["nota"])
    onda = np.zeros(n)
    for i, (fuerza, rapidez) in enumerate(ARMONICOS, start=1):
        onda += fuerza * np.exp(-t * rapidez / CAIDA_PIANO_S) * np.sin(2 * np.pi * f * i * t)
    onda *= np.minimum(t / ATAQUE_S, 1.0)
    if voz.get("soltada") is not None:
        onda *= np.exp(-np.maximum(t - voz["soltada"], 0) / SOLTAR_S)
    return (onda / 2).astype(np.float32)


def terminada(voz):
    t = voz["fase"] / TASA
    if voz["canal"] in CANALES_PERCUSION:  # suena completa aunque llegue el OFF
        return t > PERCUSION.get(voz["nota"], PERCUSION_OTRA)[1]
    if voz["canal"] == CANAL_COLCHON:
        return voz.get("soltada") is not None and t - voz["soltada"] > COLCHON_SOLTAR_S * 6
    if voz["canal"] == CANAL_VIOLIN:  # el violín se sostiene hasta que lo suelten
        return voz.get("soltada") is not None and t - voz["soltada"] > VIOLIN_SOLTAR_S * 6
    if voz.get("soltada") is not None:
        return t - voz["soltada"] > SOLTAR_S * 6
    return t > CAIDA_PIANO_S * 5


def al_pedir_audio(salida, n, _tiempo, _estado):
    mezcla = np.zeros(n, dtype=np.float32)
    ahora = time.monotonic()
    with candado:
        for clave, voz in list(voces.items()):
            if not vio_off[0] and voz.get("soltada") is None and ahora - voz["inicio"] > SIN_OFF_S:
                voz["soltada"] = voz["fase"] / TASA
            if terminada(voz):
                del voces[clave]
                continue
            g = voz["vel"] / 127 * expresion[voz["canal"]] / 127 * volumen[0] / 127
            mezcla += generar(voz, n) * g * 0.35
    salida[:, 0] = np.clip(mezcla, -1, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("puerto", nargs="?", default="/dev/cu.usbserial-110")
    parser.add_argument("--medir", action="store_true", help="muestra la hora de cada línea")
    args = parser.parse_args()
    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout = args.puerto, 115200, 0.5
    ser.dtr = ser.rts = False  # para no reiniciar la placa al abrir
    ser.open()
    print(f"Escuchando {args.puerto}. Ctrl+C para salir.")
    inicio = time.monotonic()
    # latency="low": el valor por defecto usa un búfer grande y se oye atrasado.
    with sd.OutputStream(samplerate=TASA, channels=1, callback=al_pedir_audio,
                         latency="low") as audio:
        print(f"Latencia de audio: {audio.latency * 1000:.0f} ms")
        while True:
            linea = ser.readline().decode("utf-8", "replace").strip()
            if not linea:
                continue
            m = PATRON.search(linea)
            if args.medir and (linea.startswith(("[RADIO]", "[simon]", "[dj]")) or
                               (m and m.group(1) == "ON")):
                print(f"{(time.monotonic() - inicio) * 1000:9.0f} ms  {linea}")
                if not m:
                    continue
            if not m:
                print(linea)
                continue
            orden, nums = m.group(1), [int(x) for x in m.group(2).split()]
            with candado:
                if orden == "ON" and len(nums) == 3:
                    c, n, v = nums
                    voces[(c, n)] = {"canal": c, "nota": n, "vel": v, "fase": 0,
                                     "inicio": time.monotonic()}
                elif orden == "OFF" and len(nums) == 2:
                    vio_off[0] = True
                    voz = voces.get(tuple(nums))
                    if voz is not None and voz.get("soltada") is None:
                        voz["soltada"] = voz["fase"] / TASA
                elif orden == "EXP" and len(nums) == 2:
                    expresion[nums[0]] = nums[1]
                elif orden == "VOL" and len(nums) == 1:
                    volumen[0] = nums[0]
            if orden == "VOL":
                print(linea)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
