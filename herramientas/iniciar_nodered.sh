#!/bin/sh
# Arranca Node-RED con los flujos de la orquesta. Los secretos salen de .secrets/mqtt.env y solo
# viajan como variables de entorno; no se imprimen. No toca ~/.node-red.
set -eu

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
ENV="$RAIZ/.secrets/mqtt.env"
if [ ! -f "$ENV" ]; then
  echo "No existe .secrets/mqtt.env: corre antes ./herramientas/preparar_mqtt.sh" >&2
  exit 2
fi

leer() {
  sed -n "s/^$1=//p" "$ENV" | head -1
}
for NOMBRE in NODERED_USER NODERED_PASSWORD MQTT_AES_KEY_HEX; do
  VALOR="$(leer "$NOMBRE")"
  if [ -z "$VALOR" ]; then
    echo "Falta $NOMBRE en .secrets/mqtt.env" >&2
    exit 2
  fi
  export "$NOMBRE=$VALOR"
done
unset VALOR
# Supabase es opcional: sin estas dos, el historial se queda solo en la laptop.
for NOMBRE in SUPABASE_URL SUPABASE_KEY; do
  VALOR="$(leer "$NOMBRE")"
  if [ -n "$VALOR" ]; then
    export "$NOMBRE=$VALOR"
  fi
done
unset VALOR
if [ -z "${SUPABASE_URL:-}" ] || [ -z "${SUPABASE_KEY:-}" ]; then
  echo "Sin SUPABASE_URL/SUPABASE_KEY en .secrets/mqtt.env: el historial solo se guarda en esta laptop"
fi

# Node-RED y el broker están en la misma laptop y el certificado incluye 127.0.0.1: así no importa
# si cambia la IP de la red.
export ORQUESTA_BROKER="${ORQUESTA_BROKER:-127.0.0.1}"
export ORQUESTA_BROKER_PUERTO="${ORQUESTA_BROKER_PUERTO:-8883}"
export ORQUESTA_CA="${ORQUESTA_CA:-$RAIZ/certs/ca.crt}"

# Carpeta de trabajo aparte (ignorada por git): Node-RED no reescribe los archivos del repo.
DATOS="$RAIZ/nodered/.datos"
mkdir -p "$DATOS/userdir"
chmod 700 "$DATOS"
cp "$RAIZ/nodered/flows.json" "$RAIZ/nodered/flows_cred.json" "$DATOS/userdir/"

echo "Interfaz: http://127.0.0.1:${ORQUESTA_PUERTO:-1880}/"
exec node-red --settings "$RAIZ/nodered/settings.js" --userDir "$DATOS/userdir"
