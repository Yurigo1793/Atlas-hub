#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

namespace atlas::input {

class HotkeyManager {
 public:
  bool registrarAtalhosGlobais();
  void removerAtalhosGlobais();

#ifdef _WIN32
  static constexpr int kAbrirInterface = 1;
  static constexpr int kExecutarOcr = 2;
#endif
};

}  // namespace atlas::input
