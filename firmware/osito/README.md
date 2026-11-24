# 🧸 Osito: quien lleva el ritmo

El osito recibe por ESP-NOW cifrado lo que mandan la ranita, el gatito y el pollito. Según el modo,
decide qué tocar, lo manda a la laptop para que suene y envía eventos por MQTTS al broker local
cuando hay conexión. Con el teclado 4x4 funciona además como DJ: looper, efectos y tempo. La idea
completa está en [`orquesta-proyecto.md`](../../orquesta-proyecto.md).

## Qué he podido probar

**Carga confirmada (22/11/2026):** ya quedó cargado el firmware corregido en el osito.

**Probado en físico:**

- 22/11: arranque, botón de modo, potenciómetro y LEDs de modo (Canción azul GPIO18, Libre verde
  GPIO19, Simón los dos).
- 23/11: ranita y pollito conectados por ESP-NOW; sonido en la laptop con los timbres de cada
  animal; las 16 teclas del teclado DJ (dos vueltas sin rebotes, ya con el conector volteado);
  MQTTS por el hotspot: `Wi-Fi conectado en canal 6` y `Broker conectado con TLS validado`, con el
  pollito conectado por ESP-NOW en ese mismo canal. Sin Wi-Fi, ESP-NOW arranca igual (plan B) y la
  ranita se conecta.
- Broker: TLS validado contra `certs/ca.crt`; usuario `osito` publica lo permitido, la ACL bloquea
  `control/modo` y una contraseña mala da `not authorised`.

**Simulación:** `./pruebas/correr.sh` en la compu: **154 de 154 pruebas**. Son de lógica, con stubs y
sin placa: no comprueban ESP-NOW, el cifrado, el TLS ni el hardware.

**Pendiente en físico:** el rojo del GPIO21 ante una desconexión; el looper y los efectos con sonido;
que ESP-NOW y la cola sobrevivan a una caída de Wi-Fi o del broker; la ranita en el canal 6.

## Qué hace cada módulo

| Archivo                            | Para qué está                                                           | ¿Depende de Arduino? |
| ---------------------------------- | ----------------------------------------------------------------------- | -------------------- |
| `osito.ino`                        | Llama a los módulos al arrancar                                         | sí                   |
| `pines.h`, `config.h`              | Pines, tiempos y umbrales                                               | no                   |
| `animales`                         | Recibe por ESP-NOW, muestra quién mandó qué y envía el latido del osito | sí                   |
| `interprete`                       | Valida y convierte `PayloadSombra`, `PayloadNota` y `PayloadAgitar`     | no                   |
| `estado_animales`                  | Lleva conexión (latido < 5 s), actividad (gesto < 2 s) y alertas        | no                   |
| `simulacion`                       | Acepta comandos `sim ...` por el monitor serie                          | sí                   |
| `entradas`                         | Lee el potenciómetro (GPIO34) y el botón de modo (GPIO27)               | sí                   |
| `filtro_boton`                     | Filtro local sin `delay()`; OrquestaComun ya trae `Antirrebote` (PR #5) | no                   |
| `luces`                            | LEDs 18/19 para modo y 21 para animal desconectado                      | sí                   |
| `director`, `simon`, `canciones.h` | Modos Canción, Libre y Simón Dice                                       | no                   |
| `sintetizador.h`                   | Interfaz de audio; `SintetizadorRegistro` imprime cada nota y la laptop la toca | —            |
| `teclado_dj`                       | Lee el teclado 4x4 sin bloquear, con antirrebote por tecla              | sí                   |
| `acciones_dj`                      | Qué hace cada tecla                                                     | no                   |
| `looper`, `efectos_dj`             | Looper (graba y encima capas) y efectos: eco, redoble, subida, octava, pío | no                |
| `contrato_mqtt`, `cola_eventos`    | JSON, tópicos MQTT y cola que espera el acuse                           | no                   |
| `nube`, `transporte_*`             | Cola de eventos y MQTTS real (en su propia tarea); simulador aislado    | sí                   |

## Cableado del osito

Estos son los pines del circuito que ya probamos. Yo no los cambiaría sin volver a revisar el
cableado:

| Parte         | Conexión                                 |
| ------------- | ---------------------------------------- |
| Potenciómetro | GPIO34 (ADC1)                            |
| Botón de modo | GPIO27 a GND, con `INPUT_PULLUP`         |
| LEDs de modo  | GPIO18 y GPIO19                          |
| LED de alerta | GPIO21; falta probar que prenda ante una alerta |
| Teclado DJ 4x4 | Filas 1..4 en GPIO 33, 32, 23, 22 y columnas 1..4 en 17, 16, 14, 13 (el conector quedó volteado; ya está corregido en `pines.h`) |

Uso ADC1 para el potenciómetro porque ADC2 no funciona con Wi-Fi/ESP-NOW activos.

## Lo que sale en el monitor serie

| Etiqueta            | Qué quiere decir                                                |
| ------------------- | --------------------------------------------------------------- |
| `[RADIO]`           | Llegó por ESP-NOW y pasó la validación AES-GCM y contra-replay  |
| `[SIMULACIÓN]`      | Se generó con `sim ...`; no pasó por la radio ni por el cifrado |
| `[AUDIO-REGISTRO]`  | Nota que la laptop toca (el osito no tiene bocina)              |
| `[dj]`              | Tecla del teclado DJ y lo que hizo                              |
| `[MQTTS]`           | Wi-Fi, NTP, TLS validado, conexión y estado del broker            |
| `[MQTT-SIMULACIÓN]` | Tráfico de prueba aislado; nunca se reenvía al broker real        |

Los eventos MQTT se cifran además con AES-128-GCM. El JSON exterior deja `seq` visible para el
acuse y lleva `data` como Base64 de `nonce || ciphertext || tag`; el AAD autenticado es `nodo:seq`.
Los controles y estados viajan por el canal TLS validado.

## Comandos para simular

Abre el monitor serie a **115200 baudios** y selecciona **New Line**. Puedes probar, por ejemplo:

```text
ayuda
sim vivo todos on                      latido simulado de los 3 animales cada 1 s
sim ranita sombra 60
sim gatito nota mi 90                  sim gatito nota mi 0 = soltó
sim pollito agitar 70
sim boton                              como presionar el botón de modo
sim mqtt on                            "conecta" el broker simulado
sim mqtt modo {"id":1,"modo":"libre"}
sim mqtt ack 3                         Node-RED confirma hasta el seq 3
```

## El latido del osito

La librería manda el latido por su cuenta desde los animales, pero `enlace::actualizar()` regresa
de inmediato cuando el nodo es el osito. Por eso `animales.cpp` llama
`enlace::enviarLatido()` cada `protocolo::LATIDO_CADA_MS`. Sale por difusión con la clave de grupo,
y los animales lo cuentan para `enlace::estaConectado(Nodo::Osito)`.

## Teclado DJ

```
1 silenciar ranita   2 silenciar gatito   3 silenciar pollito   A Canción
4 eco on/off         5 redoble            6 subida y caída      B Libre
7 más lento          8 octava on/off      9 más rápido          C Simón
* grabar / encimar   0 pío-pío            # play/stop loop      D borrar loop
```

El loop dura 16 pulsos (4 s a tempo normal), suena en Libre y Simón y se pausa en Canción. Mientras
graba se oye un metrónomo. Los efectos suenan en los canales 15 y 16, que el fade de Canción no toca.

## MQTTS y aprovisionamiento

1. `./herramientas/preparar_mqtt.sh` crea en `.secrets/mqtt.env` las credenciales de `osito` y
   `nodered`, `broker/passwd` (ignorado) y el certificado del broker con la IP de `BROKER_HOST`.
   Rellena `WIFI_SSID` y `WIFI_PASSWORD` (vacío si la red es abierta).
2. `mosquitto -c broker/mosquitto.conf` desde la raíz. Si Mosquitto de Homebrew corre como servicio,
   detenlo antes: ocupa el 8883.
3. Carga `firmware/aprovisionar` y corre `python3 herramientas/aprovisionar.py <puerto> osito`: graba
   las claves y el Wi-Fi/MQTTS sin mostrar ningún valor. Luego carga el osito.
4. Al arrancar, el osito imprime `Wi-Fi conectado en canal N`. Graba ese canal en cada animal
   (`aprovisionar.py <puerto> <animal> --mac-osito … --canal N`, o `canal=N` a mano).

**El ESP32 solo ve redes de 2.4 GHz.** Con el hotspot del iPhone hay que dejar abierto Ajustes →
Compartir Internet, con «Maximizar compatibilidad». Si la IP de la laptop cambia, cambia
`BROKER_HOST`, corre otra vez `preparar_mqtt.sh` (la CA no cambia), reinicia el broker y vuelve a
aprovisionar el osito. Si el Wi-Fi no conecta, el osito dice la razón y, a los 15 s, arranca ESP-NOW
igual para que la orquesta no dependa de la red.

Wi-Fi, NTP, TLS y MQTT corren en una tarea propia (núcleo 0): un broker caído no congela la música.

### Contrato MQTT del osito

| Tópico | Contenido |
|---|---|
| `orquesta/{animal}/evento` | Gestos de los animales, cifrados, por la cola con acuse |
| `orquesta/osito/evento` | `tipo:"modo"` (con `origen`: arranque, boton, teclado, nodered) y `tipo:"simon"` (pedido y resultado: acierto, error, tiempo); cifrados, por la cola con acuse |
| `orquesta/{nodo}/estado` | `{"estado":"online"}` / `{"estado":"offline"}` (LWT del osito), retenido |
| `orquesta/osito/volumen` | `{"valor":0..127}`, retenido, máximo 2 por segundo |
| `orquesta/osito/cola` | `{"pendientes":N,"descartados":D}`, retenido |
| `orquesta/control/modo` → `…/modo/ack` | Pedido de Node-RED y su acuse (Request/Response) |
| `orquesta/control/aviso` | `{"silenciar":true|false}`: calla el sonido de desconexión (el LED sigue) |
| `orquesta/sync/ack` | `{"hasta_seq":N}`: solo así se borran eventos de la cola |

Los eventos van como `{"seq":N,"data":"<Base64 de nonce || cifrado || tag>"}` con AES-128-GCM
(`MQTT_AES_KEY_HEX`) y AAD `nodo:seq`. Lo simulado (`sim boton`, `sim tecla`, `sim mqtt …`) nunca
llega al broker real, y un `sim mqtt ack` no borra la cola real.

## Pendiente de validar

- Probar que el enlace con los animales y la cola sobreviven a una caída/reconexión de Wi-Fi y del
  broker. El hotspot del iPhone elige el canal solo: revisar el canal en cada arranque.
- Node-RED: descifrar el sobre `seq`/`data`, mandar `orquesta/sync/ack` y traducir al WebSocket de
  la interfaz web (PR #13).
- Acordar la lista de canciones con la interfaz web: el osito solo tiene "prueba" y rechaza las demás.
- La canción final aún no está: `canciones.h` solo tiene "prueba".
- `seq` MQTT vuelve a 1 al encender el osito y la cola de 64 eventos vive en RAM; faltan persistencia
  y un identificador de arranque para hacer la sincronización resistente a reinicios.
- El gatito todavía usa `enlace_simulado`; `filtro_boton` podría migrar al `Antirrebote` común.
