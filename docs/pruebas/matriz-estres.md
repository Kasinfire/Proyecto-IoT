# Matriz de pruebas de estrés

Se llena al probar con las 4 placas. En **Evidencia** va el nombre del archivo de la captura o del
video, guardado en `docs/pruebas/evidencias/`. Si una prueba falla, se anota tal cual y qué se
cambió; no se borra.

Antes de empezar: firmware recién cargado, `./herramientas/iniciar_todo.sh` corriendo, sesión
abierta con un niño de prueba (alias `P-01`).

| # | Prueba | Cómo se hace | Resultado esperado | Qué se mide | Resultado | Evidencia |
|---|---|---|---|---|---|---|
| E1 | Los 3 instrumentos a la vez, 5 min | Tres personas tocan sin parar (gatito rápido, pollito agitando, mano sobre la ranita) | Suena sin cortes; la cuenta de gestos en pantalla = la del monitor serie; `descartados` = 0 | Eventos enviados vs recibidos; retraso tecla→sonido (video a cámara lenta) | | |
| E2 | Ráfaga en el gatito | Apretar las 7 teclas lo más rápido posible 30 s | Cada tecla cuenta una vez (antirrebote); nada se pierde | Teclas apretadas vs notas en pantalla | | |
| E3 | Hotspot apagado 10 min | Apagar el hotspot a mitad de E1 | La música sigue; al volver, `recuperados` > 64 (usó el desborde) y no falta ningún gesto | Pendientes máximos, tiempo en vaciar la cola, descartados | | |
| E4 | Broker detenido | Ctrl+C en Mosquitto 2 min y volver a arrancar | Pantalla avisa que no hay broker; al volver se recupera todo | Igual que E3 | | |
| E5 | Osito reiniciado sin red | Hotspot apagado, tocar 20 veces, esperar 5 s, reiniciar el osito, prender el hotspot | Al arrancar: `Recuperados de LittleFS`; los 20 gestos llegan, sin duplicados | Gestos esperados vs recibidos | | |
| E6 | Node-RED reiniciado | Ctrl+C en Node-RED a mitad de sesión y volver a arrancar | La sesión sigue abierta; el osito reenvía lo que no tenía acuse | Gestos antes/después | | |
| E7 | Laptop sin internet → Supabase | Wi-Fi de la laptop apagado, terminar la sesión, esperar 1 min, prenderlo | `nube_pendientes` sube y luego baja a 0; la sesión aparece en Supabase | Tiempo hasta que aparece | | |
| E8 | Animal apagado | Desconectar el USB del pollito | En ≤ 5 s: LED rojo del osito, tono de aviso y alerta en pantalla; la canción sigue sin percusión | Tiempo hasta la alerta en cada canal | | |
| E9 | Distancia máxima | Alejar la ranita del osito de 2 en 2 m (interior) hasta que se desconecte | Anotar a qué distancia empieza a perder latidos | Distancia (m) | | |
| E10 | Mensaje ESP-NOW alterado | Mandar un paquete con un byte cambiado (por ejemplo, modificando una copia de `prueba_enlace`) | El osito lo descarta (tag inválido) y lo cuenta | Mensajes rechazados | | |
| E11 | Mensaje repetido | Reenviar el mismo paquete ESP-NOW o MQTT dos veces | El segundo se rechaza por `(arranque, seq)` | Repetidos rechazados | | |
| E12 | Sesión larga | 45 min seguidos en modo Canción y Simón | Sin reinicios ni fugas: el monitor serie no muestra errores; memoria libre estable | Heap libre al inicio y al final (agregar un `Serial.printf` con `ESP.getFreeHeap()` para la prueba) | | |

## Pruebas automáticas (sin placas, ya pasan)

| Suite | Cómo correrla | Resultado (25/11/2026) |
|---|---|---|
| Lógica del osito | `./firmware/osito/pruebas/correr.sh` | 258 de 258 |
| Node-RED (orquesta, nube, arreglo, avatar) | `node --test nodered/pruebas/*.test.js` | 39 de 39 |

Son simulaciones: no prueban la radio, la flash ni el hardware.
