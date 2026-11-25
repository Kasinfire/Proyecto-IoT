#!/bin/sh
# Enciende broker, hora para el osito, Node-RED, página y (si el osito está por USB) el sonido. Ctrl+C apaga todo.
#   ./herramientas/iniciar_todo.sh [puerto-serie] [--sin-sonido] [--volumen=100]
# --volumen=N (0 a 127) ignora el potenciómetro del osito.
set -u
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
cd "$RAIZ" || exit 1
SONIDO=1
VOLUMEN=""
PUERTO=/dev/cu.usbserial-110
for ARG in "$@"; do
  case "$ARG" in
    --sin-sonido) SONIDO=0 ;;
    --volumen=*) VOLUMEN="$ARG" ;;
    /dev/*) PUERTO="$ARG" ;;
    *) echo "Uso: $0 [puerto-serie] [--sin-sonido] [--volumen=100]" >&2; exit 2 ;;
  esac
done

# No se cierra nada ajeno: si un puerto está ocupado, se dice quién y se termina.
for P in 8883 1880; do
  PIDS="$(lsof -tnP -iTCP:$P -sTCP:LISTEN | tr '\n' ' ')"
  if [ -n "$PIDS" ]; then
    echo "El puerto $P ya está en uso. Ciérralo con:  kill $PIDS" >&2
    exit 1
  fi
done

PID_BROKER=""
PID_NODERED=""
PID_SONIDO=""
PID_HORA=""
apagar() {
  trap - EXIT INT TERM HUP
  echo "Apagando…"
  for PID in $PID_HORA $PID_SONIDO $PID_NODERED $PID_BROKER; do kill "$PID" 2>/dev/null; done
  wait
}
trap apagar EXIT
trap 'exit 130' INT TERM HUP

# esperar <puerto> <pid> <nombre>
esperar() {
  for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    kill -0 "$2" 2>/dev/null || break
    [ -n "$(lsof -tnP -iTCP:"$1" -sTCP:LISTEN)" ] && return 0
    sleep 1
  done
  echo "$3 no arrancó; revisa el mensaje de arriba." >&2
  exit 1
}

mosquitto -c broker/mosquitto.conf &
PID_BROKER=$!
esperar 8883 "$PID_BROKER" "El broker"
python3 herramientas/servidor_hora.py &
PID_HORA=$!
./herramientas/iniciar_nodered.sh &
PID_NODERED=$!
esperar 1880 "$PID_NODERED" "Node-RED"
open http://127.0.0.1:1880/

PY="$RAIZ/.venv/bin/python"
if [ "$SONIDO" = 0 ]; then
  echo "Sin sonido, como pediste."
elif [ ! -e "$PUERTO" ]; then
  # Los puertos cambian al reconectar (el osito ha sido -110 y -210). No se abre ninguno para
  # adivinar: abrir un puerto reinicia esa placa.
  echo "SIN SONIDO: el osito no está en $PUERTO. Puertos que veo ahora:" >&2
  ls /dev/cu.usbserial-* 2>/dev/null | sed 's/^/  /' >&2 || echo "  (ninguno)" >&2
  echo "Apaga con Ctrl+C y vuelve a correr:  $0 /dev/cu.usbserial-XXX (el del osito)" >&2
elif ! "$PY" -c 'import serial, numpy, sounddevice' 2>/dev/null; then
  echo "Sin sonido: falta preparar .venv (COMANDOS.md, sección 1)."
else
  "$PY" herramientas/sintetizador.py "$PUERTO" --medir ${VOLUMEN:+"$VOLUMEN"} &
  PID_SONIDO=$!
fi
echo "Todo encendido. Ctrl+C para apagar."
wait "$PID_NODERED"
