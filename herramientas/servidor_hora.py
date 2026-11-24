"""Da la hora de esta laptop al osito (SNTP, puerto UDP 123), para que no necesite internet.

El osito la usa para validar el certificado del broker. Solo contesta a direcciones privadas y no
guarda nada. Lo enciende herramientas/iniciar_todo.sh.
"""
import ipaddress
import socket
import struct
import sys
import time

DESDE_1900 = 2208988800  # segundos entre 1900 (NTP) y 1970 (Unix)
CLIENTE = 3
SERVIDOR = 4
TAMANO = 48


def sello(segundos):
    entero = int(segundos)
    return struct.pack("!II", (entero + DESDE_1900) & 0xFFFFFFFF, int((segundos - entero) * 2**32))


def responder(pedido, llego, ahora):
    """La respuesta, o None si no es una pregunta de cliente."""
    if len(pedido) < TAMANO or pedido[0] & 7 != CLIENTE:
        return None
    version = pedido[0] >> 3 & 7
    # Estrato 10: reloj local, para que nadie lo prefiera sobre uno de verdad.
    cabeza = struct.pack("!BBbb", version << 3 | SERVIDOR, 10, pedido[2] if pedido[2] < 18 else 6, -20)
    return (cabeza + struct.pack("!II4s", 0, 0, b"LOCL") + sello(llego) + pedido[40:48]
            + sello(llego) + sello(ahora))


def main():
    puerto = int(sys.argv[1]) if len(sys.argv) > 1 else 123
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        try:
            s.bind(("", puerto))
        except OSError as error:
            sys.exit(f"No se pudo abrir el puerto UDP {puerto}: {error}")
        print(f"Dando la hora en el puerto UDP {puerto}", flush=True)
        while True:
            pedido, quien = s.recvfrom(512)
            if not ipaddress.ip_address(quien[0]).is_private:
                continue
            respuesta = responder(pedido, time.time(), time.time())
            if respuesta:
                s.sendto(respuesta, quien)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
