#pragma once

#include <functional>
#include <optional>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
using ColorRef = unsigned int;
using UiByte = unsigned char;
#endif

#include "core/screen_capture.hpp"
#include "input/hotkey_manager.hpp"

namespace atlas::ui {

class AppWindow {
 public:
  struct UiSettings {
    std::wstring fontName = L"Press Start 2P";
    int fontSizePt = 11;
#ifdef _WIN32
    COLORREF fontColor = RGB(255, 255, 255);
    COLORREF panelColor = RGB(47, 79, 111);
    BYTE windowOpacity = 245;
#else
    ColorRef fontColor = 0x00FFFFFF;
    ColorRef panelColor = 0x006F4F2F;
    UiByte windowOpacity = 245;
#endif
  };

  using OcrCallback = std::function<std::string(const std::optional<atlas::core::CaptureRegion>&)>;

  explicit AppWindow(OcrCallback onOcrRequest);
  bool criar(void* instanceHandle = nullptr);
  int executarLoop();
  void atualizarResultado(const std::string& texto);

 private:
#ifdef _WIN32
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  void desenharBotao(LPDRAWITEMSTRUCT drawInfo);
  void organizarLayout(HWND hwnd);
  void aplicarTemaVisual(HWND hwnd);
  void aplicarProximaConfiguracaoVisual();
  std::string descreverConfiguracaoAtual() const;
  std::optional<atlas::core::CaptureRegion> selecionarRegiaoTela();
  void executarOcrPorSelecao();
#endif

  OcrCallback onOcrRequest_;
  UiSettings uiSettings_{};
  atlas::input::HotkeyManager hotkeyManager_;
#ifdef _WIN32
  HWND hwnd_ = nullptr;
  HWND outputText_ = nullptr;
  HWND ocrButton_ = nullptr;
  HWND configButton_ = nullptr;
  HWND sairButton_ = nullptr;
  HFONT uiFont_ = nullptr;
#endif
};

}  // namespace atlas::ui
