#pragma once

#include <cstdint>
#include <vector>

#include "core/screen_capture.hpp"

namespace atlas::core {

struct BitmapPixels {
  int width = 0;
  int height = 0;
  int stride = 0;
  std::vector<std::uint8_t> pixelsBGRA;
};

BitmapPixels converterBitmapParaPixels(const CapturedImage& image);

}  // namespace atlas::core
