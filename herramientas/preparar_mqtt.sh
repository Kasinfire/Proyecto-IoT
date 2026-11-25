#!/bin/sh
# Crea credenciales locales del broker, su password_file y la CA/certificado TLS.
# No imprime secretos; WIFI_SSID y WIFI_PASSWORD se rellenan localmente en .secrets/mqtt.env.
set -eu

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
SECRETS="$RAIZ/.secrets"
ENV="$SECRETS/mqtt.env"
PASSWD="$RAIZ/broker/passwd"
HOST_INICIAL="${1:-}"
mkdir -p "$SECRETS" "$RAIZ/broker"
chmod 700 "$SECRETS"

if [ ! -f "$ENV" ]; then
  if [ -z "$HOST_INICIAL" ]; then
    HOST_INICIAL="$(ipconfig getifaddr en0 2>/dev/null || true)"
  fi
  if [ -z "$HOST_INICIAL" ]; then
    echo "No pude detectar la IP. Ejecuta: $0 <IP-de-esta-Mac>" >&2
    exit 2
  fi
  python3 - "$ENV" "$HOST_INICIAL" <<'PY'
from pathlib import Path
import os, secrets, sys
path = Path(sys.argv[1])
rows = [
    "WIFI_SSID=",
    "WIFI_PASSWORD=",
    f"BROKER_HOST={sys.argv[2]}",
    "MQTT_USER=osito",
    f"MQTT_PASSWORD={secrets.token_urlsafe(32)}",
    f"MQTT_AES_KEY_HEX={secrets.token_hex(16)}",
    "NODERED_USER=nodered",
    f"NODERED_PASSWORD={secrets.token_urlsafe(32)}",
]
path.write_text("\n".join(rows) + "\n", encoding="utf-8")
os.chmod(path, 0o600)
PY
fi
chmod 600 "$ENV"

python3 - "$ENV" "$PASSWD" <<'PY'
from pathlib import Path
import base64, hashlib, os, secrets, sys
values = {}
for line in Path(sys.argv[1]).read_text(encoding="utf-8").splitlines():
    if line and not line.lstrip().startswith("#") and "=" in line:
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip()
required = ("MQTT_USER", "MQTT_PASSWORD", "NODERED_USER", "NODERED_PASSWORD")
if any(not values.get(key) for key in required):
    raise SystemExit("Faltan usuarios/secretos en .secrets/mqtt.env; no se generó passwd.")

def entry(user, password):
    salt = secrets.token_bytes(12)
    digest = hashlib.pbkdf2_hmac("sha512", password.encode(), salt, 100000, dklen=64)
    b64 = lambda data: base64.b64encode(data).decode("ascii")
    return f"{user}:$7$100000${b64(salt)}${b64(digest)}"

out = Path(sys.argv[2])
out.write_text("\n".join((entry(values["MQTT_USER"], values["MQTT_PASSWORD"]),
                         entry(values["NODERED_USER"], values["NODERED_PASSWORD"]))) + "\n",
               encoding="ascii")
os.chmod(out, 0o600)
PY

HOST="$(python3 - "$ENV" <<'PY'
from pathlib import Path
import sys
for line in Path(sys.argv[1]).read_text(encoding="utf-8").splitlines():
    if line.startswith("BROKER_HOST="):
        print(line.split("=", 1)[1].strip())
        break
PY
)"
if [ -z "$HOST" ]; then
  echo "Define BROKER_HOST en .secrets/mqtt.env y vuelve a ejecutar el script." >&2
  exit 2
fi
"$RAIZ/certs/generar_certs.sh" "$HOST"
echo "Usuario osito y ACL listos. Completa Wi-Fi localmente en .secrets/mqtt.env."
