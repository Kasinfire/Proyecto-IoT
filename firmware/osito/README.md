# 🧸 Osito: gateway y director musical

Recibe a la ranita, el gatito y el pollito por ESP-NOW cifrado, decide qué suena según el modo
y (cuando haya broker) reporta a Node-RED. La especificación está en
[`orquesta-proyecto.md`](../../orquesta-proyecto.md).

## Qué hace cada módulo

| Archivo | Qué hace | ¿Usa Arduino? |
|---|---|---|
| `osito.ino` | Solo llama a los módulos | sí |
| `pines.h`, `config.h` | Pines (los del cableado probado) y todos los tiempos/umbrales | no |
| `animales` | Recibe por ESP-NOW, imprime quién mandó qué, manda el latido del osito | sí |
| `interprete` | Valida y convierte `PayloadSombra`/`PayloadNota`/`PayloadAgitar` | no |
| `estado_animales` | Conectado (latido < 5 s), activo (gesto < 2 s), alerta | no |
| `simulacion` | Comandos `sim ...` por el monitor serie | sí |
| `entradas` | Potenciómetro (GPIO34) y botón de modo (GPIO27) | sí |
| `filtro_boton` | Antirrebote sin `delay()` (posible duplicado del PR #5) | no |
| `luces` | LEDs 18/19 = modo, 21 = alerta de animal desconectado | sí |
| `director`, `simon`, `canciones.h` | Modos Canción, Libre y Simón Dice | no |
| `sintetizador.h` | Interfaz de audio; hoy solo `SintetizadorRegistro` (imprime, no suena) | — |
| `contrato_mqtt`, `cola_eventos` | JSON y tópicos de MQTT, cola que espera el acuse | no |
| `nube`, `transporte_*` | Puente al broker; hoy solo `TransporteSimulado` | sí |

Lo que dice "no" se prueba en la compu, sin placa: `./pruebas/correr.sh`.

## Cómo distinguir lo real de lo simulado en el monitor serie

| Etiqueta | Significa |
|---|---|
| `[RADIO]` | Llegó por ESP-NOW y pasó el tag AES-GCM y el contra-replay |
| `[SIMULACIÓN]` | Lo escribiste tú con `sim ...`; se salta la radio y el cifrado |
| `[AUDIO-REGISTRO]` | Lo que sonaría; **no hay audio real todavía** |
| `[MQTT-SIMULACIÓN]` | Lo que se publicaría; **nada sale de la placa** |

Lo simulado nunca se publica en un broker real (`TransporteMqtt::esSimulado`).

## Comandos de simulación (115200, "New Line")

```
ayuda
sim vivo todos on                      latido simulado de los 3 animales cada 1 s
sim ranita sombra 60
sim gatito nota mi 90                  sim gatito nota mi 0  = soltó
sim pollito agitar 70
sim boton                              como presionar el botón de modo
sim mqtt on                            "conecta" el broker simulado
sim mqtt modo {"id":1,"modo":"libre"}
sim mqtt ack 3                         Node-RED confirma hasta el seq 3
```

## El latido del osito

La librería manda el latido solo desde los animales: `enlace::actualizar()` sale de inmediato si
el nodo es el osito. Sin cambiar la librería, `animales.cpp` llama `enlace::enviarLatido()` cada
`protocolo::LATIDO_CADA_MS`; sale por difusión con la clave de grupo y en los animales cuenta para
`enlace::estaConectado(Nodo::Osito)`.

## Pendientes (no inventados)

- **Canción final:** `canciones.h` solo tiene "prueba" (una nota por parte, no es canción).
- **MQTTS real:** falta dirección del broker, CA (`ca_cert.h` de la Práctica 4), usuario/clave
  del osito, SSID/clave del Wi-Fi, nombres de esas llaves en NVS y cómo cargarlas. Lo que debe
  hacer está en `transporte_mqtt.h`. Con Wi-Fi, `animales::iniciar()` va **después** de conectar
  al router, para que ESP-NOW use su canal.
- **Cifrado de eventos MQTT:** acordar con Node-RED la clave y qué va como AAD.
- **Campos propuestos:** `nivel`, `intensidad` y `{"estado":"online"}` (sin RSSI); acordarlos.
- **`seq` de MQTT reinicia en 1** al prender el osito: Node-RED lo tomaría como duplicado.
  Guardarlo en NVS o agregar `arranque` al contrato.
- **Cola en LittleFS:** hoy vive en RAM (64 eventos) y se pierde al reiniciar.
- **Animales:** la ranita no lee `PayloadModo` ni `PayloadLuz` todavía; el gatito (PR #6) usa
  enlace simulado; el pollito no tiene firmware.
- **Antirrebote:** cuando entre el PR #5, cambiar `filtro_boton` por `Antirrebote` de OrquestaComun.
