# Pendientes de OrquestaComun

Utilidades que usa más de un animal y que van como plantillas en esta librería, no copiadas en cada sketch. Sigan el [estándar de código](../../../docs/estandar-codigo.md).

Cada una va en su propio `.h` dentro de `src/` y se integra con un PR que revisa alguien de otro animal.

---

## 1. `Antirrebote` — `src/antirrebote.h`

**Para qué:** que un botón o sensor digital reporte un solo cambio aunque el contacto rebote varias veces.

**Usan:** gatito (teclas), pollito (SW-520D), osito (botón de modo).

**Interfaz esperada:**

```cpp
class Antirrebote {
 public:
  Antirrebote(uint8_t pin, uint32_t estable_ms, bool activo_en_bajo);
  void iniciar();      // configura el pin (INPUT_PULLUP si activo_en_bajo)
  void actualizar();   // llamar en cada loop(), sin delay()
  bool presionado() const;
  bool sePresiono();   // true solo una vez por flanco de bajada/subida
  bool seSolto();
};
```

**Cuidados:**
- Sin `delay()`: comparar contra `millis()`.
- El tiempo estable viene del `config.h` de cada animal, no fijo en la clase.
- GPIO 34–39 no tienen pull-up interno: si alguien usa uno, el pull-up va en el circuito.
- No necesita ser plantilla; si varios animales necesitan arreglos de antirrebotes, se usa `Antirrebote[]`.

**Listo cuando:** presionar una tecla 20 veces cuenta exactamente 20 en el monitor serie.
