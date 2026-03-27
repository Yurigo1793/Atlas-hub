#include "input/hotkey_manager.hpp"

namespace atlas::input {

bool HotkeyManager::registrarAtalhosGlobais() {
#ifdef _WIN32
  const bool abriu = RegisterHotKey(nullptr, kAbrirInterface, MOD_CONTROL | MOD_SHIFT, 'A') != 0;
  const bool ocr = RegisterHotKey(nullptr, kExecutarOcr, MOD_CONTROL | MOD_SHIFT, VK_OEM_5) != 0;
  return abriu && ocr;
#else
  return false;
#endif
}

void HotkeyManager::removerAtalhosGlobais() {
#ifdef _WIN32
  UnregisterHotKey(nullptr, kAbrirInterface);
  UnregisterHotKey(nullptr, kExecutarOcr);
#endif
}

}  // namespace atlas::input
