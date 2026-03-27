#include <atomic>
#include <iostream>
#include <optional>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

#include "core/ocr_engine.hpp"
#include "core/screen_capture.hpp"
#include "ui/app_window.hpp"
#include "utils/command_loop.hpp"

int main() {
  std::cout << "Atlas-Hub iniciado\n";

  atlas::core::OcrEngine ocrEngine;
  auto executarOcrTela = [&ocrEngine](const std::optional<atlas::core::CaptureRegion>& regiao) {
    auto captura = regiao.has_value() ? atlas::core::capturarTela(*regiao) : atlas::core::capturarTela();
    std::string texto = ocrEngine.reconhecerTexto(captura);
    captura.release();
    return texto;
  };

  std::atomic<bool> uiPronta{false};
  std::thread uiThread([&]() {
#ifdef _WIN32
    atlas::ui::AppWindow window(executarOcrTela);
    uiPronta = window.criar(GetModuleHandleW(nullptr));
    if (uiPronta) {
      window.executarLoop();
    }
#else
    uiPronta = false;
#endif
  });

  atlas::utils::CommandLoop loopComandos(executarOcrTela);
  loopComandos.executar();

#ifdef _WIN32
  PostQuitMessage(0);
#endif

  if (uiThread.joinable()) {
    uiThread.join();
  }

  return 0;
}
