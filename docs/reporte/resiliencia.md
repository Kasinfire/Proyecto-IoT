# Resiliencia (Unidad 5)

El sistema tolera tres tipos de falla sin perder datos para siempre. La música nunca depende de la
red: todo lo que suena se decide en el osito.

## Nivel 1: osito → broker (caída de Wi-Fi o del broker, reinicio del osito)

```
 evento ──► cola en RAM (64) ──► publicar MQTT ──► Node-RED guarda ──► orquesta/sync/ack {arranque, hasta_seq}
               │  llena                                                        │
               ▼                                                               ▼
        /desborde.bin (LittleFS, hasta 4000, en orden)                 el osito borra hasta ahí
               │
        sin acuse 3 s: copia de la RAM en /cola.bin (cada 1 s)  ──► al encender se recupera
```

- **Cada evento lleva `seq` y `arranque`.** `seq` empieza en 1 en cada encendido y `arranque` es un
  número al azar por encendido; Node-RED lleva la cuenta por arranque, así que los eventos que el
  osito recupera tras un reinicio no se confunden con los nuevos.
- **Nada se borra sin acuse.** Publicar no basta (QoS 0): solo el acuse de Node-RED, que se manda
  después de guardar. El desborde tampoco se borra de la flash hasta que todo lo suyo tiene acuse.
- **Al reconectar** el osito vuelve a publicar todo en orden; Node-RED descarta duplicados.
- **Capacidad:** 64 en RAM + 4000 en flash, unos 30 min sin red a ~2 eventos por segundo.
- **Con red no se escribe en la flash**: la copia solo se hace cuando los acuses dejan de llegar.

Límites honestos:

- Si el desborde también se llena, el evento nuevo se descarta y se cuenta (`orquesta/osito/cola`,
  campo `descartados`). Igual un registro dañado en la flash.
- Si se va la luz del osito, se pierde lo que entró a la RAM en el último segundo, o en los últimos
  3 s si todavía había red.

Código: `firmware/osito/cola_eventos.*`, `firmware/osito/cola_persistente.*`, `firmware/osito/nube.cpp`,
`nodered/orquesta.js` (`alSobre`, `syncDe`, `confirmar`). Pruebas: `firmware/osito/pruebas/correr.sh`
(apartado "cola") y `nodered/pruebas/orquesta.test.js`.

## Nivel 2: laptop → Supabase (sin internet en la laptop o Supabase pausado)

- Cada alta o cambio de niño y cada sesión terminada se anota en una cola (`paraLaNube`) que se
  guarda en el archivo de Node-RED antes de responder.
- Cada 10 s se manda en orden; solo se quita lo que Supabase confirmó con 2xx. Si falla, se reintenta
  a los 10, 20, 40 y luego cada 60 s.
- Sobrevive a reiniciar Node-RED. El historial se sigue viendo en la interfaz, porque sale de la
  copia local.

Código: `nodered/nube.js`, `nodered/orquesta.js` (`alaNube`, `sincronizarNube`). Pruebas:
`nodered/pruebas/nube.test.js`.

## Nivel 0: animal ↔ osito (ESP-NOW)

Si un animal se apaga o sale de alcance, el osito lo marca desconectado tras 5 s sin latido, avisa
por tres canales (LED, sonido y pantalla) y la canción sigue sin esa parte. Al volver, se reconecta
solo.

## Evidencia que hay que sacar (con las placas)

Para cada prueba: captura de la interfaz **antes**, **durante** y **después**, y el monitor serie del
osito.

1. **Hotspot apagado 10 min** a mitad de una sesión, tocando los animales. Durante: la pantalla dice
   que no hay conexión y el monitor serie muestra `pendientes` creciendo (pasa de 64, así que se
   usa el desborde). Después: la pantalla muestra `recuperados: N` y la cuenta de gestos de la sesión
   incluye los del corte.
2. **Osito reiniciado sin red**: apagar el hotspot, tocar ~20 veces, esperar 5 s, desconectar y
   conectar el USB del osito, prender el hotspot. El monitor serie al arrancar dice
   `Recuperados de LittleFS: N en RAM…` y esos gestos llegan a la sesión.
3. **Broker detenido** (Ctrl+C en Mosquitto) y vuelto a arrancar: igual que el 1.
4. **Laptop sin internet**: terminar una sesión con el Wi-Fi apagado; la consola de Node-RED dice
   `Supabase: sin conexión… Quedan N por subir`. Al volver, aparece la fila en el Table Editor de
   Supabase.
