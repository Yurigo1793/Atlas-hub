#include "utils/command_loop.hpp"

#include <iostream>
#include <utility>

namespace atlas::utils {

CommandLoop::CommandLoop(OcrCommand ocrCommand) : ocrCommand_(std::move(ocrCommand)) {}

void CommandLoop::executar() {
  imprimirAjuda();

  for (std::string comando; std::cout << "atlas> " && std::getline(std::cin, comando);) {
    if (comando == "sair") {
      std::cout << "Encerrando Atlas-Hub...\n";
      break;
    }

    if (comando == "ajuda") {
      imprimirAjuda();
      continue;
    }

    if (comando == "ocr_tela") {
      if (ocrCommand_) {
        std::cout << "--- Resultado OCR ---\n" << ocrCommand_(std::nullopt)
                  << "\n---------------------\n";
      }
      continue;
    }

    std::cout << "Comando desconhecido. Digite 'ajuda' para listar os comandos.\n";
  }
}

void CommandLoop::imprimirAjuda() const {
  std::cout << "Comandos disponiveis:\n"
            << "  ajuda      - exibe esta lista\n"
            << "  ocr_tela   - captura a tela e executa OCR\n"
            << "  sair       - encerra o programa\n";
}

}  // namespace atlas::utils
