#pragma once

#include <functional>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace atlas::ui {

class AppWindow {
 public:
  using OcrCallback = std::function<std::string()>;

  explicit AppWindow(OcrCallback onOcrRequest);
  bool criar(void* instanceHandle = nullptr);
  int executarLoop();
  void atualizarResultado(const std::string& texto);

 private:
#ifdef _WIN32
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  void desenharBotao(LPDRAWITEMSTRUCT drawInfo);
#endif

  OcrCallback onOcrRequest_;
#ifdef _WIN32
  HWND hwnd_ = nullptr;
  HWND outputText_ = nullptr;
#endif
};

}  // namespace atlas::ui
