// La cola en LittleFS con apagones, en la compu. SIMULACIÓN: la flash es memoria; no prueba la placa.
#include <cstdio>
#include <cstdlib>
#include <set>

// Se incluyen las fuentes para poder "apagar" el osito (borrar su RAM) dejando la flash.
#include "../cola_eventos.cpp"
#include "../cola_persistente.cpp"

uint32_t reloj_ms = 0;

namespace {
int fallos = 0;
int pruebas = 0;
uint32_t arranque = 0;
std::set<uint64_t> enviados;      // lo que el niño hizo
std::set<uint64_t> en_node_red;   // lo que Node-RED guardó

void revisar(const char* nombre, bool paso) {
  ++pruebas;
  fallos += paso ? 0 : 1;
  printf("[SIMULACIÓN] %-66s %s\n", nombre, paso ? "OK" : "FALLO");
}

uint64_t clave(uint32_t a, uint32_t seq) { return (static_cast<uint64_t>(a) << 32) | seq; }

void pasar(uint32_t ms) {
  for (uint32_t i = 0; i < ms; i += 50) {
    reloj_ms += 50;
    cola_persistente::actualizar();
  }
}

void encender() {
  cola_eventos::usarDesborde(nullptr);
  cola_eventos::reiniciar();
  cola_persistente::desborde = cola_persistente::DesbordeArchivo();
  cola_persistente::hay_copia = false;
  cola_persistente::version_guardada = cola_persistente::leidos_guardados = 0;
  cola_persistente::ultima_copia_ms = cola_persistente::frente_desde_ms = 0;
  cola_persistente::frente_arranque = cola_persistente::frente_seq = 0;
  reloj_ms = 0;
  ++arranque;
  cola_persistente::iniciar();
}

void flashNueva() {
  LittleFS.archivos.clear();
  LittleFS.caben = SIZE_MAX;
  enviados.clear();
  en_node_red.clear();
  encender();
}

void evento() {
  const uint32_t seq = cola_eventos::reservarSeq();
  char json[60];
  snprintf(json, sizeof(json), "{\"seq\":%u,\"arranque\":%u}", seq, arranque);
  cola_eventos::agregar(arranque, seq, 1, json);
  enviados.insert(clave(arranque, seq));
}

void eventos(int cuantos, uint32_t cada_ms = 200) {
  for (int i = 0; i < cuantos; ++i) {
    evento();
    pasar(cada_ms);
  }
}

// Publica uno y Node-RED lo confirma. false si no hay o si el registro no se podría cifrar.
bool entregar(bool confirmar = true) {
  uint32_t a = 0;
  uint32_t seq = 0;
  uint8_t nodo = 0;
  const char* json = nullptr;
  if (!cola_eventos::siguientePorPublicar(a, seq, nodo, json)) return false;
  char esperado[60];
  snprintf(esperado, sizeof(esperado), "{\"seq\":%u,\"arranque\":%u}", seq, a);
  if (strcmp(json, esperado) != 0 || nodo != 1) return false;
  cola_eventos::marcarPublicado(a, seq);
  if (confirmar) {
    // Como Node-RED: el acuse llega hasta donde no falta ninguno.
    en_node_red.insert(clave(a, seq));
    uint32_t hasta = 0;
    while (en_node_red.count(clave(a, hasta + 1)) || !enviados.count(clave(a, hasta + 1))) {
      if (++hasta >= seq) break;
    }
    cola_eventos::confirmarHasta(a, hasta);
  }
  return true;
}

void entregarTodo() {
  cola_eventos::republicarTodo();
  while (entregar()) {
  }
}

bool hayDesborde() { return LittleFS.exists("/desborde.bin"); }

void apagonAMediaDescarga() {
  flashNueva();
  eventos(100);
  pasar(5000);
  while (cola_eventos::pendientes() > 40) {
    entregar();
    pasar(50);
  }
  revisar("lo que salio del desborde sigue en la flash hasta su acuse", hayDesborde());
  encender();  // se fue la luz
  entregarTodo();
  revisar("apagon a media descarga: no se pierde ninguno", en_node_red == enviados);
  revisar("ya confirmado todo, se borra el desborde", !hayDesborde());
  pasar(100);
  revisar("y la copia de la RAM", !LittleFS.exists("/cola.bin"));
}

void desbordeNuevoConCopiaVieja() {
  flashNueva();
  eventos(100);
  pasar(5000);
  while (cola_eventos::pendientes() > 64) entregar();
  for (int i = 0; i < 4; ++i) entregar(false);  // salen, pero el acuse no llega
  pasar(5000);                                  // se copia con algunos ya leidos
  while (cola_eventos::pendientes() > 0) entregarTodo();
  // /cola.bin todavia dice "36 leidos" de un archivo que ya no existe.
  for (int i = 0; i < 100; ++i) evento();
  revisar("hay un desborde nuevo", hayDesborde() && cola_persistente::desborde.cantidad() == 36);
  encender();
  entregarTodo();
  size_t ultimos = 0;
  for (const uint64_t k : enviados) {
    const uint32_t s = static_cast<uint32_t>(k);
    ultimos += s > 164 && en_node_red.count(k) ? 1 : 0;
  }
  revisar("el 'ya salieron' de otro archivo no se salta eventos", ultimos == 36);
}

void escrituraAMedias() {
  flashNueva();
  for (int i = 0; i < 65; ++i) evento();
  LittleFS.caben = 50;
  evento();  // el 66 no cupo
  enviados.erase(clave(arranque, 66));
  LittleFS.caben = SIZE_MAX;
  for (int i = 0; i < 5; ++i) evento();
  revisar("el que no se pudo escribir se cuenta", cola_eventos::descartados() == 1);
  entregarTodo();
  revisar("los siguientes llegan enteros y en orden", en_node_red == enviados);

  flashNueva();
  for (int i = 0; i < 70; ++i) evento();
  std::string& archivo = LittleFS.archivos["/desborde.bin"];
  memset(&archivo[8 + 2 * sizeof(cola_eventos::Registro)], 0xFF, 40);  // flash dañada
  enviados.erase(clave(arranque, 67));  // ese ya no existe: Node-RED se lo salta tras la espera
  entregarTodo();
  revisar("un registro danado se salta, se cuenta y no detiene la cola",
          cola_eventos::pendientes() == 0 && en_node_red.size() == 69 &&
              cola_eventos::descartados() == 1);

  flashNueva();
  LittleFS.archivos["/desborde.bin"] = "basura";
  LittleFS.archivos["/cola.bin"] = "basura";
  encender();
  eventos(70, 10);
  entregarTodo();
  revisar("con archivos que no son nuestros arranca limpio", en_node_red == enviados);
}

void desbordeLleno() {
  flashNueva();
  for (size_t i = 0; i < config::CAPACIDAD_COLA_MQTT + config::DESBORDE_MAX + 3; ++i) evento();
  revisar("lleno: se descartan los nuevos y se cuentan", cola_eventos::descartados() == 3);
  entregarTodo();
  revisar("lleno: lo demas llega", en_node_red.size() == config::CAPACIDAD_COLA_MQTT + config::DESBORDE_MAX);
}

// Muchas caídas de red y apagones en momentos al azar. Antes de cada apagón pasa el tiempo en que
// la copia ya debe existir; lo que aún no tenía acuse debe llegar después.
void alAzar() {
  bool todo = true;
  for (unsigned semilla = 1; semilla <= 400; ++semilla) {
    srand(semilla);
    flashNueva();
    for (int vuelta = 0; vuelta < 6; ++vuelta) {
      eventos(rand() % 150, 50 + rand() % 300);
      pasar(6000);
      cola_eventos::republicarTodo();
      for (int n = rand() % 220; n > 0; --n) {
        entregar(rand() % 8 != 0);
        if (rand() % 3 == 0) pasar(50 * (rand() % 40));
        if (rand() % 20 == 0) cola_eventos::republicarTodo();
      }
      if (rand() % 3 != 0) encender();
    }
    for (int i = 0; i < 5; ++i) entregarTodo();
    todo = todo && en_node_red == enviados && cola_eventos::pendientes() == 0 && !hayDesborde();
    if (!todo) {
      printf("  semilla %u: faltan %zu\n", semilla, enviados.size() - en_node_red.size());
      break;
    }
  }
  revisar("400 historias al azar con apagones: no falta ningun evento", todo);
}
}  // namespace

int main() {
  apagonAMediaDescarga();
  desbordeNuevoConCopiaVieja();
  escrituraAMedias();
  desbordeLleno();
  alAzar();
  printf("[SIMULACIÓN] %d de %d pruebas de la cola en LittleFS pasaron\n", pruebas - fallos, pruebas);
  return fallos == 0 ? 0 : 1;
}
