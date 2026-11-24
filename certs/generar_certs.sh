#!/bin/sh
# Genera la CA privada local y un certificado de servidor con SAN para el host del broker.
set -eu

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
SECRETS="$RAIZ/.secrets"
HOST="${1:-}"
if [ -z "$HOST" ]; then
  echo "Uso: $0 <IP-o-DNS-del-broker>" >&2
  exit 2
fi
mkdir -p "$SECRETS"
chmod 700 "$SECRETS"

if [ -e "$SECRETS/ca.key" ] || [ -e "$RAIZ/certs/ca.crt" ]; then
  if [ ! -f "$SECRETS/ca.key" ] || [ ! -f "$RAIZ/certs/ca.crt" ]; then
    echo "CA incompleta: conserva o restaura ca.key y ca.crt juntos; no se sobrescribió nada." >&2
    exit 1
  fi
else
  openssl req -x509 -newkey rsa:3072 -sha256 -nodes -days 3650 \
    -keyout "$SECRETS/ca.key" -out "$RAIZ/certs/ca.crt" \
    -subj "/CN=Orquesta IoT Local CA" \
    -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" \
    -addext "subjectKeyIdentifier=hash" >/dev/null 2>&1
fi
chmod 600 "$SECRETS/ca.key"

TMP="$(mktemp -d "${TMPDIR:-/tmp}/orquesta-cert.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
if python3 - "$HOST" <<'PY' >/dev/null 2>&1
import ipaddress, sys
ipaddress.ip_address(sys.argv[1])
PY
then
  SAN="IP.1 = $HOST"
else
  SAN="DNS.1 = $HOST"
fi
cat >"$TMP/server.cnf" <<EOF
[req]
prompt = no
distinguished_name = dn
req_extensions = server_ext
[dn]
CN = $HOST
[server_ext]
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names
[alt_names]
$SAN
IP.2 = 127.0.0.1
DNS.2 = localhost
EOF

if [ ! -f "$SECRETS/broker.key" ]; then
  openssl genrsa -out "$SECRETS/broker.key" 2048 >/dev/null 2>&1
fi
chmod 600 "$SECRETS/broker.key"
openssl req -new -sha256 -key "$SECRETS/broker.key" -out "$TMP/broker.csr" \
  -config "$TMP/server.cnf" >/dev/null 2>&1
openssl x509 -req -sha256 -days 825 -in "$TMP/broker.csr" \
  -CA "$RAIZ/certs/ca.crt" -CAkey "$SECRETS/ca.key" -CAcreateserial \
  -out "$RAIZ/certs/broker.crt" -extfile "$TMP/server.cnf" -extensions server_ext \
  >/dev/null 2>&1
chmod 644 "$RAIZ/certs/ca.crt" "$RAIZ/certs/broker.crt"
openssl verify -CAfile "$RAIZ/certs/ca.crt" "$RAIZ/certs/broker.crt" >/dev/null

python3 - "$RAIZ/certs/ca.crt" "$RAIZ/firmware/osito/ca_cert.h" <<'PY'
from pathlib import Path
import sys
pem = Path(sys.argv[1]).read_text(encoding="ascii")
header = (
    "// Certificado público de la CA local; la clave privada correspondiente vive en .secrets/.\n"
    "#pragma once\n\n"
    "static const char CA_CERT_PEM[] PROGMEM = R\"ORQUESTACA(\n"
    + pem
    + ")ORQUESTACA\";\n"
)
Path(sys.argv[2]).write_text(header, encoding="utf-8")
PY

echo "Certificado TLS preparado para $HOST; las llaves privadas permanecen en .secrets/."
