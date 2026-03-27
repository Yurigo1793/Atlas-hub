#pragma once

#include <string>

#include "core/screen_capture.hpp"

namespace atlas::core {

class OcrEngine {
 public:
  std::string reconhecerTexto(const CapturedImage& image) const;
};

}  // namespace atlas::core
