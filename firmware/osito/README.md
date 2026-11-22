# 🧸 Osito: quien lleva el ritmo

El osito recibe por ESP-NOW cifrado lo que mandan la ranita, el gatito y el pollito. Según el modo,
decide qué tocar; más adelante, cuando tengamos el broker, también mandará eventos a Node-RED.
La idea completa está en [`orquesta-proyecto.md`](../../orquesta-proyecto.md).

## Qué he podido probar

**Compilación (22/11/2026):** el firmware compiló y llegó a `esptool.py v5.3.1`. La carga a la
placa sigue en curso; falta confirmar que termine y que el osito arranque con este firmware. El
puerto USB aparece, pero eso por sí solo no confirma que la carga haya quedado bien.

**Simulación:** corrí `./pruebas/correr.sh` en la compu y pasaron **81 de 81 pruebas**. Son pruebas
de lógica, con stubs y sin placa: no comprueban ESP-NOW, el cifrado ni el hardware real.

**Comprobado en físico:** por separado ya se habían probado el potenciómetro, el botón, los LEDs,
el PAM8403 y la bocina. Todavía falta validar el conjunto con este firmware, incluida la radio y el
audio; esas pruebas no se dan por hechas mientras la carga siga en curso.

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
| `filtro_boton`                     | Quita rebotes sin usar `delay()`; podría duplicar el PR #5              | no                   |
| `luces`                            | LEDs 18/19 para modo y 21 para animal desconectado                      | sí                   |
| `director`, `simon`, `canciones.h` | Modos Canción, Libre y Simón Dice                                       | no                   |
| `sintetizador.h`                   | Interfaz de audio; `SintetizadorRegistro` solo imprime, no suena        | —                    |
| `contrato_mqtt`, `cola_eventos`    | JSON, tópicos MQTT y cola que espera el acuse                           | no                   |
| `nube`, `transporte_*`             | Puente al broker; por ahora usamos `TransporteSimulado`                 | sí                   |

## Cableado del osito

Estos son los pines del circuito que ya probamos. Yo no los cambiaría sin volver a revisar el
cableado:

| Parte         | Conexión                                 |
| ------------- | ---------------------------------------- |
| Potenciómetro | GPIO34 (ADC1)                            |
| Botón de modo | GPIO27 a GND, con `INPUT_PULLUP`         |
| LEDs de modo  | GPIO18 y GPIO19                          |
| LED de alerta | GPIO21; avisa si se desconecta un animal |

Uso ADC1 para el potenciómetro porque ADC2 no funciona con Wi-Fi/ESP-NOW activos.

## Lo que sale en el monitor serie

| Etiqueta            | Qué quiere decir                                                |
| ------------------- | --------------------------------------------------------------- |
| `[RADIO]`           | Llegó por ESP-NOW y pasó la validación AES-GCM y contra-replay  |
| `[SIMULACIÓN]`      | Se generó con `sim ...`; no pasó por la radio ni por el cifrado |
| `[AUDIO-REGISTRO]`  | Esto es lo que sonaría; todavía no es audio real                |
| `[MQTT-SIMULACIÓN]` | Esto se publicaría; no sale nada de la placa                    |

`TransporteMqtt::esSimulado` evita que la simulación publique en un broker real.

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

## Lo que falta

- **Canción final:** `canciones.h` solo tiene "prueba", una nota por parte; todavía no es una canción.
- **MQTTS real:** faltan la dirección del broker, la CA (`ca_cert.h` de la Práctica 4), usuario y
  clave del osito, SSID y clave del Wi-Fi, y acordar cómo guardar y cargar esas llaves en NVS. El
  flujo está descrito en `transporte_mqtt.h`. Con Wi-Fi, `animales::iniciar()` debe ir después de
  conectar al router, para que ESP-NOW use el mismo canal.
- **Cifrado de eventos MQTT:** falta acordar con Node-RED la clave y qué va como AAD.
- **Campos de MQTT:** falta acordar `nivel`, `intensidad` y `{"estado":"online"}` (sin RSSI).
- **Secuencia MQTT:** `seq` vuelve a 1 al prender el osito; Node-RED podría tomarlo como duplicado.
  Falta guardarlo en NVS o agregar `arranque` al contrato.
- **Persistencia de la cola:** los 64 eventos viven en RAM y se pierden al reiniciar; falta pasarlos
  a LittleFS.
- **Animales y botón:** la ranita todavía no lee `PayloadModo` ni `PayloadLuz`; el gatito (PR #6)
  usa enlace simulado y el pollito aún no tiene firmware. Cuando entre el PR #5, cambiaré
  `filtro_boton` por `Antirrebote` de OrquestaComun.
