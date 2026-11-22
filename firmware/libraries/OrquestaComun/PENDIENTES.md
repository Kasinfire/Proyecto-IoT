# Pendientes de OrquestaComun

Utilidades que usa más de un animal y que van como plantillas en esta librería, no copiadas en cada sketch. Sigan el [estándar de código](../../../docs/estandar-codigo.md).

Cada una va en su propio `.h` dentro de `src/` y se integra con un PR que revisa alguien de otro animal.

---

## 1. `ColaCircular<T, N>` — `src/cola_circular.h`

**Para qué:** el callback de recepción de ESP-NOW corre en otra tarea y debe salir rápido. Solo mete el mensaje en la cola; `actualizar()` lo saca y lo procesa en el `loop()`.

**Usan:** los 4 (también el osito para el buffer de eventos antes de escribir a LittleFS).

**Interfaz esperada:**

```cpp
template <typename T, size_t N>
class ColaCircular {
 public:
  bool meter(const T& elemento);  // false si está llena (no sobrescribe)
  bool sacar(T& elemento);        // false si está vacía
  bool vacia() const;
  bool llena() const;
  size_t tamano() const;
};
```

**Cuidados:**
- Se escribe desde el callback y se lee desde `loop()`: proteger con `portMUX_TYPE` + `portENTER_CRITICAL` / `portEXIT_CRITICAL`, o documentar por qué no hace falta.
- Sin memoria dinámica (`new`, `malloc`, `std::vector`): arreglo fijo de `N`.

**Listo cuando:** un sketch de prueba mete más de `N` elementos, `meter` devuelve `false` al llenarse y los saca en el mismo orden.

---

## 3. `Antirrebote` — `src/antirrebote.h`

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

---

## 4. `enlace::enviar<T>(payload)` — `src/enlace.h` / `src/enlace.cpp`

**Para qué:** que cada animal mande un mensaje con una sola línea y que no pueda equivocarse de tipo ni de tamaño:

```cpp
enlace::enviar(protocolo::PayloadNota{protocolo::aEntero(protocolo::Nota::Mi), 90});
```

**Usan:** los 4. **Depende de:** `cifrado_aes` y `credenciales` (lo hace quien haga el cifrado).

**Interfaz esperada:**

```cpp
namespace enlace {
void iniciar();      // canal Wi-Fi y claves leídos de NVS, registra al par
void actualizar();   // procesa lo que dejó el callback en la ColaCircular

template <typename T>
bool enviar(const T& payload);  // arma Cabecera con protocolo::TipoDe<T>::valor
bool enviarLatido();             // Latido no tiene payload
}
```

**Cuidados:**
- `enviar<T>` solo arma la cabecera y llama a una función no plantilla (`enviarCrudo(tipo, datos, tamano)`) en el `.cpp`; ahí va el cifrado, para no meter toda la lógica en el `.h`.
- Rellenar la cabecera con `protocolo::VERSION`, `arranque` (NVS) y `seq` incremental.
- Clave de enlace propia para animal → osito; clave de grupo para osito → animales (ver encabezado de `protocolo.h`).
- Rechazar en la recepción cualquier `(arranque, seq)` que no sea mayor al último aceptado de ese emisor.

**Listo cuando:** el osito recibe y descifra mensajes de un animal, y un mensaje repetido o con un byte alterado se rechaza (evidencia para la matriz de pruebas).
