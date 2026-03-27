#pragma once

#include <functional>
#include <optional>
#include <string>

#include "core/screen_capture.hpp"

namespace atlas::utils {

class CommandLoop {
 public:
  using OcrCommand = std::function<std::string(const std::optional<atlas::core::CaptureRegion>&)>;

  explicit CommandLoop(OcrCommand ocrCommand);
  void executar();

 private:
  void imprimirAjuda() const;

  OcrCommand ocrCommand_;
};

}  // namespace atlas::utils
