// LittleFS en memoria. `caben` limita los bytes de la siguiente escritura (flash llena).
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

class File {
 public:
  File() = default;
  File(std::string* datos, size_t* caben) : datos_(datos), caben_(caben) {}
  explicit operator bool() const { return datos_ != nullptr; }
  size_t size() const { return datos_ ? datos_->size() : 0; }
  void close() { datos_ = nullptr; }
  bool seek(size_t lugar) {
    lugar_ = lugar;
    return datos_ != nullptr;
  }
  size_t read(uint8_t* destino, size_t n) {
    if (!datos_ || lugar_ >= datos_->size()) return 0;
    const size_t k = std::min(n, datos_->size() - lugar_);
    memcpy(destino, datos_->data() + lugar_, k);
    lugar_ += k;
    return k;
  }
  size_t write(const uint8_t* origen, size_t n) {
    const size_t k = std::min(n, *caben_);
    if (datos_->size() < lugar_ + k) datos_->resize(lugar_ + k);
    memcpy(&(*datos_)[lugar_], origen, k);
    lugar_ += k;
    return k;
  }

 private:
  std::string* datos_ = nullptr;
  size_t* caben_ = nullptr;
  size_t lugar_ = 0;
};

struct LittleFSFalso {
  std::map<std::string, std::string> archivos;
  size_t caben = SIZE_MAX;
  bool begin(bool) { return true; }
  bool exists(const char* ruta) { return archivos.count(ruta) > 0; }
  File open(const char* ruta, const char* modo) {
    if (modo[0] == 'r' && !exists(ruta)) return File();
    if (modo[0] == 'w') archivos[ruta].clear();
    return File(&archivos[ruta], &caben);
  }
  bool remove(const char* ruta) { return archivos.erase(ruta) > 0; }
  bool rename(const char* de, const char* a) {
    archivos[a] = archivos[de];
    return archivos.erase(de) > 0;
  }
};
inline LittleFSFalso LittleFS;
