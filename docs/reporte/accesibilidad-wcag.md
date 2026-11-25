# Auditoría de accesibilidad (WCAG 2.2)

Los contrastes se calcularon con la fórmula de luminancia relativa de WCAG sobre los colores reales
de `web/css/base.css` y de cada regla que define color de texto y fondo. Los tamaños salen del CSS.
Falta confirmarlo con una herramienta en el navegador (Lighthouse o WebAIM Contrast Checker) y
agregar capturas.

## 1.4.3 Contraste del texto (mínimo 4.5:1; 3:1 texto grande)

| Texto | Fondo | Contraste | Resultado |
|---|---|---|---|
| Tinta `#1c1c1a` | Blanco | 17.07 | Cumple |
| Tinta | Todos los fondos pastel (lima, rosa, lila, animales, avisos) | 11.88 – 15.74 | Cumple |
| Tinta suave `#4a4a44` | Todos los fondos | 6.21 – 8.92 | Cumple |
| Tinta tenue `#5f5f58` | Blanco, superficie, relleno | 5.68 – 6.43 | Cumple |
| Tinta tenue | Lila del gatito `#d9d2ff` | **4.48** | No cumple por poco, si se llega a usar ahí (hoy no hay una regla que los junte). Arreglo: oscurecer la tinta tenue a `#5a5a53` (4.84:1) |
| Blanco | Botón principal violeta `#6858d4` | 5.34 | Cumple |
| Blanco | Botones de alerta, aviso y bien | 8.06 – 11.62 | Cumple |
| Texto de alerta `#6e1238` | Fondo de alerta `#ffd9e7` | 9.04 | Cumple |
| Blanco con sombra oscura | Cielo de la pantalla del niño `#86ccf3` | 1.76 sin la sombra | **Parcial**: es texto grande (22 px, Press Start 2P) con sombra de 3 px `#2a3a4a`, que es lo que lo hace legible. WCAG no cuenta la sombra de forma clara; arreglo seguro: poner el texto sobre un bloque oscuro como los demás letreros |
| Blanco | Letrero café `#8a5a30` (pantalla del niño) | 5.9 | Cumple |

## 1.4.11 Contraste de componentes y gráficos (mínimo 3:1)

| Elemento | Contra | Contraste | Resultado |
|---|---|---|---|
| Colores fuertes de los animales en gráficas (osito, ranita, gatito, pollito) | Blanco | 3.50 – 5.34 | Cumple |
| Los mismos | Superficie `#f6f6f1` | 3.23 – 4.92 | Cumple |
| Foco del teclado: contorno violeta de 3 px | Blanco | 5.34 | Cumple |
| Modo elegido: fondo lima | Fondo de los no elegidos | 1.10 | El fondo solo no basta, **pero** el radio del modo elegido cambia a un círculo con borde oscuro de 6 px (`.modo[aria-checked="true"] .modo__circulo`): cumple por el círculo |
| Página actual en el menú: fondo lima | Blanco | 1.25 | **Parcial**: también se pone en negritas (800). Arreglo: subrayado o borde inferior oscuro |
| Borde del radio sin elegir `#8a8a82` | Blanco | 3.5 aprox. | Cumple |

## 2.5.8 Tamaño del objetivo (mínimo 24 × 24 px)

- Todos los botones, enlaces del menú, entradas y opciones usan `--objetivo-min: 44px` de alto
  (`web/css/base.css`), casi el doble de lo que pide 2.5.8 (y lo que pide 2.5.5 AAA).
- Los cuadros más chicos (palomita de confirmado de 22 px, círculo del modo de 18 px) **no son
  objetivos**: están dentro de un botón de 44 px.
- En la pantalla del niño no se toca nada: el niño interactúa con los animales físicos.
- Los animales: teclas de foamy del gatito de varios centímetros; el pollito y la ranita no tienen
  objetivos pequeños.

## Otros criterios revisados

- **1.4.1 Uso del color:** cada color de animal va con su nombre o su carita; el estado conectado o
  desconectado se dice con texto.
- **2.4.7 Foco visible:** `:focus-visible` con contorno de 3 px.
- **4.1.3 Mensajes de estado:** la alerta de desconexión es `role="alert"` y los errores de los
  formularios también; los contadores usan `aria-live="polite"`.

## Estado crítico por más de un canal sensorial

**Animal desconectado** (sin latido ESP-NOW por 5 s):

| Canal | Cómo | Dónde en el código |
|---|---|---|
| Visual en el peluche | LED rojo del osito parpadea | `firmware/osito/luces.cpp` (`luces::alerta`), llamado desde `osito.ino` |
| Auditivo | Tono de aviso por el sintetizador de la laptop | `director::avisoDesconexion()` en `firmware/osito/director.cpp` |
| Visual en pantalla | Alerta con la carita del animal y texto, `role="alert"` (también la leen los lectores de pantalla) | `web/en-vivo.html`, `web/js/en-vivo.js` (`pintarAlerta`) |

El terapeuta puede silenciar el tono desde la pantalla (`POST /api/aviso`); el LED sigue. Pendiente:
probarlo con la placa (está en la matriz de estrés).
