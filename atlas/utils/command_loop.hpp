#pragma once

#include <functional>
#include <string>

namespace atlas::utils {

class CommandLoop {
 public:
  using OcrCommand = std::function<std::string()>;

  explicit CommandLoop(OcrCommand ocrCommand);
  void executar();

 private:
  void imprimirAjuda() const;

  OcrCommand ocrCommand_;
};

}  // namespace atlas::utils
