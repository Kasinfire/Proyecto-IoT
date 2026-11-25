# Costo y autonomía energética

> Precios aproximados del mercado mexicano (tiendas de electrónica y marketplaces, noviembre 2026).
> **Verificar con los tickets reales antes de entregar.** Los consumos son de hoja de datos y
> referencias típicas del ESP32; **pendiente medirlos** con un multímetro USB en serie con cada nodo.

## Costo: variante barata (la que se construyó)

| Componente | Cantidad | Precio unitario aprox. (MXN) | Subtotal |
|---|---|---|---|
| ESP32 DevKit (38 pines) | 4 | 150 | 600 |
| Protoboard 830 puntos (2 por ESP32; el pollito usa una de 170) | 7 + 1 | 40 / 20 | 300 |
| Cables dupont (macho-macho y hembra-macho) | 2 juegos | 45 | 90 |
| Botones táctiles (gatito) + botón de modo (osito) | 8 | 2 | 16 |
| LEDs 5 mm y resistencias 220 Ω | 14 | 2 | 28 |
| Fotorresistencia (LDR) + resistencia 10 kΩ | 1 | 8 | 8 |
| Sensor de inclinación SW-520D | 1 | 15 | 15 |
| Potenciómetro 10 kΩ | 1 | 10 | 10 |
| Teclado matricial 4x4 (osito) | 1 | 35 | 35 |
| Amplificador PAM8403 + bocina pequeña (o bocina bluetooth) | 1 | 60 | 60 |
| Foamy, unicel (esfera del pollito), pegamento | — | — | 100 |
| **Total** | | | **≈ 1,260** |

Sin las 4 ESP32 y las protoboards, que el equipo ya tenía, el gasto real fue de ~$200–230 MXN
(`orquesta-proyecto.md`, sección 10). La laptop del consultorio no se cuenta: ya existe.

## Costo: componentes reales (variante original)

| Componente | Para qué | Precio aprox. (MXN) |
|---|---|---|
| 4 × LiPo 3.7 V 1000 mAh + 4 × cargador TP4056 | Quitar el cable USB | 4 × (90 + 20) = 440 |
| VL53L0X (ranita) | Distancia de la mano en lugar de la sombra | 90 |
| 7 botones arcade 30 mm (gatito) | Teclas grandes y resistentes | 7 × 25 = 175 |
| MPU6050 (pollito) | Acelerómetro en lugar del SW-520D | 60 |
| Motor vibrador + 2N2222 (pollito) | Respuesta táctil | 30 |
| SAM2695 o VS1053 + bocina 8 Ω 3 W (osito) | Sintetizador en el osito, sin laptop para el sonido | 450 |
| Tira WS2812 (1 m) | LEDs de color en todos | 120 |
| **Total adicional** | | **≈ 1,365** |

Con las ESP32 y lo demás, la variante original ronda **$1,700–2,000 MXN** para los cuatro animales.

## Consumo y autonomía

| Nodo | Qué está encendido | Corriente típica | Autonomía con LiPo 1000 mAh (≈ 800 mAh útiles) |
|---|---|---|---|
| Ranita, gatito, pollito | ESP32 con radio ESP-NOW siempre escuchando + 1–2 LEDs | 90–110 mA | ≈ 7–9 h |
| Osito | Wi-Fi + MQTTS + ESP-NOW + LEDs | 130–160 mA (picos de 240 mA al transmitir) | ≈ 5–6 h |

Cálculo: autonomía = capacidad útil / corriente promedio. Se usa el 80 % de la batería por la
eficiencia del regulador y para no descargar la LiPo a fondo.

Una sesión de musicoterapia dura 30–45 min: con una carga, cada animal aguanta un día completo de
consultas (8–10 sesiones). Hoy todo se alimenta por USB (laptop o power bank), y un power bank
común de 10,000 mAh alimenta un animal varios días.

### Qué bajaría el consumo

- **Modem sleep / light sleep** en los animales entre gestos: ESP-NOW necesita la radio encendida
  para recibir el modo y la luz de Simón, así que el ahorro está en apagarla entre latidos y
  despertar con el sensor (GPIO). Podría bajar a 20–40 mA.
- Apagar los LEDs cuando nadie toca.
- El osito no puede dormir: es el gateway.

## ¿Es viable con pocos recursos?

**Sí, para un consultorio o una escuela pública:**

- La variante barata cuesta ~$1,260 MXN con todo nuevo; un instrumento terapéutico comercial
  adaptado cuesta varias veces eso.
- Funciona sin internet: la música, los modos y la sesión siguen en la laptop. La nube es opcional
  (el plan gratis de Supabase alcanza).
- Las piezas se consiguen en cualquier tienda de electrónica y se arman sin soldar.

**Límites:** hace falta una laptop para el sonido y la interfaz (la variante original con SAM2695
quitaría la del sonido), y sin baterías los animales dependen de cables USB, que estorban a un niño
que se mueve.
