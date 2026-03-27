#include "input/hotkey_manager.hpp"

namespace atlas::input {

bool HotkeyManager::registrarAtalhosGlobais(void* windowHandle) {
#ifdef _WIN32
  HWND hwnd = reinterpret_cast<HWND>(windowHandle);
  const bool abriu = RegisterHotKey(hwnd, kAbrirInterface, MOD_CONTROL | MOD_SHIFT, 'A') != 0;
  const bool ocr = RegisterHotKey(hwnd, kExecutarOcr, MOD_CONTROL | MOD_SHIFT, VK_OEM_5) != 0;
  return abriu && ocr;
#else
  (void)windowHandle;
  return false;
#endif
}

void HotkeyManager::removerAtalhosGlobais(void* windowHandle) {
#ifdef _WIN32
  HWND hwnd = reinterpret_cast<HWND>(windowHandle);
  UnregisterHotKey(hwnd, kAbrirInterface);
  UnregisterHotKey(hwnd, kExecutarOcr);
#else
  (void)windowHandle;
#endif
}

}  // namespace atlas::input
