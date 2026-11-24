"""Graba en la NVS de una placa los datos de .secrets/, sin escribirlos a mano ni mostrarlos.

Antes: cargar firmware/aprovisionar en la placa y cerrar cualquier monitor serie.
Después: volver a cargar el firmware del animal o del osito (la NVS se conserva).

  Osito (claves de los 3 animales + Wi-Fi/MQTTS de .secrets/mqtt.env):
    python3 herramientas/aprovisionar.py /dev/cu.usbserial-110 osito

  Animal (su clave + la MAC del osito + el canal Wi-Fi que imprime el osito al conectarse):
    python3 herramientas/aprovisionar.py /dev/ttyUSB0 ranita --mac-osito AA:BB:CC:DD:EE:FF --canal 6

Lee .secrets/credenciales.env (clave_grupo, clave_ranita, ...) y, para el osito,
.secrets/mqtt.env (WIFI_SSID, WIFI_PASSWORD, BROKER_HOST, MQTT_USER, MQTT_PASSWORD,
MQTT_AES_KEY_HEX). Solo imprime el nombre de cada dato y la respuesta de la placa.
Necesita pyserial: python3 -m pip install pyserial
"""
import argparse
import re
import sys
import time
from pathlib import Path

try:
    import serial
except ImportError:
    sys.exit("Falta pyserial: python3 -m pip install pyserial")

RAIZ = Path(__file__).resolve().parent.parent
ANIMALES = ("ranita", "gatito", "pollito")
LINEA_MAX = 127  # LINEA_MAX - 1 del sketch aprovisionar
# Nombre en mqtt.env -> comando del sketch aprovisionar.
MQTT = {
    "WIFI_SSID": "wifi_ssid",
    "WIFI_PASSWORD": "wifi_password",
    "BROKER_HOST": "broker_host",
    "MQTT_USER": "mqtt_user",
    "MQTT_PASSWORD": "mqtt_password",
    "MQTT_AES_KEY_HEX": "mqtt_aes_key",
}
PATRON_MAC = re.compile(r"^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$")


def leer_env(ruta):
    if not ruta.is_file():
        sys.exit(f"No existe {ruta}")
    valores = {}
    for linea in ruta.read_text(encoding="utf-8").splitlines():
        linea = linea.strip()
        if linea and not linea.startswith("#") and "=" in linea:
            nombre, _, valor = linea.partition("=")
            valores[nombre.strip()] = valor.strip().strip('"')
    return valores


def armar_lineas(nodo, secretos, mac_osito, canal):
    """Lista de (nombre para mostrar, línea a mandar). Nunca se imprime la línea."""
    claves = leer_env(secretos / "credenciales.env")
    necesarias = ["clave_grupo"] + (
        [f"clave_{a}" for a in ANIMALES] if nodo == "osito" else [f"clave_{nodo}"])
    lineas = []
    for nombre in necesarias:
        if not claves.get(nombre):
            sys.exit(f"Falta {nombre} en credenciales.env")
        lineas.append((nombre, f"{nombre}={claves[nombre]}"))

    if nodo == "osito":
        mqtt = leer_env(secretos / "mqtt.env")
        for nombre_env, comando in MQTT.items():
            # WIFI_PASSWORD vacío = red abierta; los demás son obligatorios.
            if nombre_env not in mqtt or (not mqtt[nombre_env] and nombre_env != "WIFI_PASSWORD"):
                sys.exit(f"Falta {nombre_env} en mqtt.env (rellénalo y vuelve a correr)")
            lineas.append((comando, f"{comando}={mqtt.get(nombre_env, '')}"))
    else:
        if not mac_osito or not PATRON_MAC.match(mac_osito):
            sys.exit("Para un animal: --mac-osito AA:BB:CC:DD:EE:FF (la imprime el osito)")
        if canal is None or not 1 <= canal <= 13:
            sys.exit("Para un animal: --canal 1..13 (el que imprime el osito al conectarse)")
        lineas.append(("mac_osito", f"mac_osito={mac_osito.upper()}"))
        lineas.append(("canal", f"canal={canal}"))

    for nombre, linea in lineas:
        if len(linea) > LINEA_MAX or "\n" in linea:
            sys.exit(f"{nombre} es demasiado largo para el sketch aprovisionar")
    return lineas


def respuesta(puerto, segundos):
    fin = time.monotonic() + segundos
    texto = b""
    while time.monotonic() < fin:
        texto += puerto.read(256)
    return texto.decode("utf-8", "replace").strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("puerto")
    parser.add_argument("nodo", choices=("osito",) + ANIMALES)
    parser.add_argument("--mac-osito")
    parser.add_argument("--canal", type=int)
    parser.add_argument("--secretos", type=Path, default=RAIZ / ".secrets",
                        help="carpeta con credenciales.env y mqtt.env")
    parser.add_argument("--prueba", action="store_true",
                        help="solo revisa los archivos y dice qué mandaría (sin abrir el puerto)")
    args = parser.parse_args()

    lineas = armar_lineas(args.nodo, args.secretos, args.mac_osito, args.canal)
    if args.prueba:
        print(f"Para {args.nodo} mandaría: {', '.join(n for n, _ in lineas)}, ver")
        return

    with serial.Serial(args.puerto, 115200, timeout=0.1) as puerto:
        print("Reiniciando la placa y esperando al sketch aprovisionar...")
        # Pulso en EN por RTS (como el monitor serie): abrir el puerto no siempre la reinicia.
        puerto.dtr = False
        puerto.rts = True
        time.sleep(0.1)
        puerto.rts = False
        inicio = respuesta(puerto, 6.0)
        if "Aprovisionamiento" not in inicio:
            puerto.write(b"ver\n")  # quizá ya estaba corriendo: le pregunto
            if "clave_grupo" not in respuesta(puerto, 1.5):
                sys.exit("La placa no respondió como el sketch aprovisionar: cárgalo primero.")
        fallas = 0
        for nombre, linea in lineas:
            puerto.write((linea + "\n").encode("utf-8"))
            dijo = respuesta(puerto, 1.0)
            fallas += "guardado" not in dijo
            print(f"-> {nombre}: {dijo}")  # el sketch nunca repite el valor
        puerto.write(b"ver\n")
        print("-> ver:")
        print(respuesta(puerto, 1.5))
    if fallas:
        sys.exit(f"{fallas} dato(s) no se guardaron; revisa los mensajes de arriba.")


if __name__ == "__main__":
    main()
