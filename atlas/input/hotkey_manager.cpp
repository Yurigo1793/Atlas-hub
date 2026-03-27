#include "input/hotkey_manager.hpp"

#include <string>

namespace atlas::input {

bool HotkeyManager::registrarAtalhosGlobais(void* windowHandle) {
#ifdef _WIN32
  (void)windowHandle;
  const bool abriu = RegisterHotKey(nullptr, kAbrirInterface, MOD_CONTROL | MOD_SHIFT, 'A') != 0;
  if (!abriu) {
    const DWORD erro = GetLastError();
    const std::wstring mensagem =
        L"Falha ao registrar hotkey Ctrl+Shift+A. Erro WinAPI: " + std::to_wstring(erro);
    MessageBoxW(nullptr, mensagem.c_str(), L"Atlas-Hub", MB_OK | MB_ICONERROR);
  }

  const bool ocr = RegisterHotKey(nullptr, kExecutarOcr, MOD_CONTROL | MOD_SHIFT, VK_OEM_5) != 0;
  if (!ocr) {
    const DWORD erro = GetLastError();
    const std::wstring mensagem =
        L"Falha ao registrar hotkey Ctrl+Shift+\\. Erro WinAPI: " + std::to_wstring(erro);
    MessageBoxW(nullptr, mensagem.c_str(), L"Atlas-Hub", MB_OK | MB_ICONERROR);
  }

  return abriu && ocr;
#else
  (void)windowHandle;
  return false;
#endif
}

void HotkeyManager::removerAtalhosGlobais(void* windowHandle) {
#ifdef _WIN32
  (void)windowHandle;
  UnregisterHotKey(nullptr, kAbrirInterface);
  UnregisterHotKey(nullptr, kExecutarOcr);
#else
  (void)windowHandle;
#endif
}

}  // namespace atlas::input
