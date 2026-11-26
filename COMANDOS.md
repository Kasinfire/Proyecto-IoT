# Comandos de la orquesta

Todos se corren desde la carpeta del proyecto:

```sh
cd ~/Projects/Proyecto-IoT
```

Los puertos cambian al reconectar: revisa con `ls /dev/cu.usbserial*` y usa el que toque en lugar de los de los ejemplos (osito `-110`, gatito `-10`).

En VS Code, espera a que aparezca `(.venv)` antes de correr algo en una terminal nueva: al activarlo manda un Ctrl+C.

## 1. Encender todo

**Un solo comando** (Ctrl+C lo apaga todo):

```sh
./herramientas/iniciar_todo.sh
```

Con `--volumen=100` (0 a 127) suena aunque el potenciómetro falle. Con `--sin-sonido` no abre el sintetizador. Si el osito está en otro puerto, ponlo al final: `./herramientas/iniciar_todo.sh /dev/cu.usbserial-210`.

O por separado, cada uno en su propia terminal y en este orden:

Broker:

```sh
mosquitto -c broker/mosquitto.conf
```

Hora para el osito (sin ella no puede validar el certificado si no hay internet):

```sh
python3 herramientas/servidor_hora.py
```

Node-RED (debe decir `Connected to broker`):

```sh
./herramientas/iniciar_nodered.sh
```

Página:

```sh
open http://127.0.0.1:1880/
```

Sonido del osito (abrir el puerto puede reiniciar la placa):

```sh
.venv/bin/python herramientas/sintetizador.py /dev/cu.usbserial-110 --medir
```

El sonido y el aprovisionamiento necesitan esto una sola vez:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install pyserial numpy sounddevice
```

Páginas: `/en-vivo.html`, `/ninos.html`, `/historial.html`, `/canciones.html`, `/proyector.html`. Con `?ensayo` al final usan datos simulados, sin placas.

## 2. Apagar, o si dice «port in use»

Primero Ctrl+C en cada terminal. Si algo sigue abierto, busca su PID (segunda columna) y ciérralo:

```sh
lsof -nP -iTCP:1880 -iTCP:8883 -sTCP:LISTEN
kill PID
```

Si el 8883 lo ocupa el Mosquitto de Homebrew corriendo como servicio (usa otro certificado), detenlo:

```sh
brew services stop mosquitto
```

Quién ocupa el puerto serie del osito:

```sh
lsof /dev/cu.usbserial-110
```

## 3. Placas

Cierra antes el sintetizador y cualquier monitor serie. Cargar **sobrescribe el firmware** de la placa, pero conserva sus claves.

Ver puertos:

```sh
arduino-cli board list
```

Compilar sin cargar (cambia `osito` por `ranita`, `gatito` o `pollito`):

```sh
arduino-cli compile -b esp32:esp32:esp32 --warnings all --libraries firmware/libraries firmware/osito
```

Cargar el osito:

```sh
arduino-cli compile --upload --fqbn esp32:esp32:esp32:EraseFlash=none,UploadSpeed=115200 --port /dev/cu.usbserial-110 --libraries firmware/libraries firmware/osito
```

Cargar el gatito:

```sh
arduino-cli compile --upload --fqbn esp32:esp32:esp32:EraseFlash=none,UploadSpeed=115200 --port /dev/cu.usbserial-10 --libraries firmware/libraries firmware/gatito
```

Monitor serie (se sale con Ctrl+C):

```sh
arduino-cli monitor -p /dev/cu.usbserial-110 -c baudrate=115200
```

Reiniciar sin cargar: botón **EN** de la placa.

## 4. Cambiar de red o de IP

1. Conecta la Mac a la red nueva y mira su IP:

   ```sh
   ipconfig getifaddr en0
   ```

2. Cambia `WIFI_SSID`, `WIFI_PASSWORD` y `BROKER_HOST` (la IP del paso 1). Guardar: Ctrl+O, Enter, Ctrl+X.

   ```sh
   nano .secrets/mqtt.env
   ```

3. Rehaz el certificado del broker (conserva la misma CA):

   ```sh
   ./herramientas/preparar_mqtt.sh
   ```

4. Reinicia Mosquitto y Node-RED (secciones 2 y 1).
5. Aprovisiona el osito (sección 5).

La red debe ser de 2.4 GHz. La que mejor ha funcionado es la de la propia Mac: Ajustes → General → Compartir → Compartir Internet, desde el USB del iPhone, para Wi-Fi, canal 6 y WPA2. Entonces `BROKER_HOST` es `192.168.2.1` (en el paso 1, `ifconfig bridge100 | grep inet`). El osito no necesita internet: la hora se la da `servidor_hora.py`.

## 5. Aprovisionar una placa

Los animales solo se repiten si cambia el canal Wi-Fi.

### Osito

1. Cargar el sketch de aprovisionar (sobrescribe el firmware por un momento):

   ```sh
   arduino-cli compile --upload --fqbn esp32:esp32:esp32:EraseFlash=none,UploadSpeed=115200 --port /dev/cu.usbserial-110 --libraries firmware/libraries firmware/aprovisionar
   ```

2. Grabar los datos (no muestra las claves; con `--prueba` solo revisa los archivos):

   ```sh
   .venv/bin/python herramientas/aprovisionar.py /dev/cu.usbserial-110 osito
   ```

3. Volver a cargar el firmware del osito (sección 3).

### Un animal (ejemplo: gatito)

Usa la MAC y el canal que imprime el osito al conectarse.

1. Cargar el sketch de aprovisionar:

   ```sh
   arduino-cli compile --upload --fqbn esp32:esp32:esp32:EraseFlash=none,UploadSpeed=115200 --port /dev/cu.usbserial-10 --libraries firmware/libraries firmware/aprovisionar
   ```

2. Grabar los datos:

   ```sh
   .venv/bin/python herramientas/aprovisionar.py /dev/cu.usbserial-10 gatito --mac-osito MAC_DEL_OSITO --canal 6
   ```

3. Volver a cargar el firmware del animal (sección 3).

## 6. Pruebas

Osito y Node-RED, en SIMULACIÓN:

```sh
bash firmware/osito/pruebas/correr.sh
node --test nodered/pruebas/*.test.js
```

Canciones del firmware al día (sin `--revisar` las vuelve a generar):

```sh
node herramientas/generar_canciones.mjs --revisar
```

Capturas del README (`docs/capturas/`), en modo ensayo y sin placas. Con un servidor de la carpeta `web/`
corriendo (`python3 -m http.server 8765 --directory web`), en otra terminal:

```sh
for p in en-vivo historial ninos canciones proyector; do
  "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new --hide-scrollbars \
    --force-device-scale-factor=1.5 --window-size=1440,900 --virtual-time-budget=8000 \
    --screenshot="docs/capturas/$p.png" "http://127.0.0.1:8765/$p.html?ensayo"
done
```

El proyector se ve completo con `--window-size=1440,810`. La matriz de pruebas de estrés con las placas
está en [`docs/pruebas/matriz-estres.md`](docs/pruebas/matriz-estres.md).

## 7. Diagnóstico

¿El osito está en el broker? Debe salir una IP que no sea `127.0.0.1`:

```sh
lsof -nP -iTCP:8883 -sTCP:ESTABLISHED
```

Calidad de la red (cambia la IP por la de arriba):

```sh
ping -c 30 IP_DEL_OSITO
```

En el monitor del osito, la señal debe ser de unos -67 dBm o mejor. Con -80 dBm o peor se corta.

| Razón | Significa |
|---|---|
| 201 | No ve la red: lejos, solo 5 GHz o nombre mal |
| 2 | Se agotó el tiempo al autenticarse: suele ser señal débil |
| 15, 202 o 204 | Suele ser la contraseña |

## 8. Git

No uses `git add .`: metería `certs/broker.crt` y `orquesta-v3.html`.

```sh
git status
git add web nodered firmware herramientas broker/mosquitto.conf broker/acl
git commit -m "Mensaje"
git push
gh pr view 16 --web
```

## 9. Supabase (una sola vez)

Sin esto todo funciona igual, pero el historial solo queda en la laptop.

1. En <https://supabase.com> crea un proyecto (plan gratis, región más cercana).
2. En **SQL Editor** pega el contenido de `nodered/supabase.sql` y dale **Run**.
3. En **Project Settings → API Keys** copia la *Project URL* y una **secret key** (`sb_secret_…`).
   Nunca la publishable/anon: con RLS no deja escribir.
4. Agrégalas al final de `.secrets/mqtt.env` (no se sube a git):

   ```sh
   nano .secrets/mqtt.env
   ```

   ```
   SUPABASE_URL=https://xxxxxxxx.supabase.co
   SUPABASE_KEY=sb_secret_xxxxxxxx
   ```

5. Reinicia Node-RED. La primera vez sube todo lo que ya había. Revisa en **Table Editor** que
   aparezcan `ninos`, `sesiones`, `canciones` (las 13 incluidas y las tuyas) y la vista `historial`.

Los proyectos gratis se pausan tras una semana sin uso: ábrelo en el navegador el día antes de la demo.
