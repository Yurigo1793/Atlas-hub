#include "core/image_processing.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace atlas::core {

BitmapPixels converterBitmapParaPixels(const CapturedImage& image) {
  BitmapPixels output;
#ifdef _WIN32
  if (image.bitmap == nullptr || image.width <= 0 || image.height <= 0) {
    return output;
  }

  HDC hdc = GetDC(nullptr);
  if (hdc == nullptr) {
    return output;
  }

  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = image.width;
  info.bmiHeader.biHeight = -image.height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;

  output.width = image.width;
  output.height = image.height;
  output.stride = image.width * 4;
  output.pixelsBGRA.resize(static_cast<size_t>(output.stride * output.height));

  if (GetDIBits(
          hdc,
          image.bitmap,
          0,
          static_cast<UINT>(image.height),
          output.pixelsBGRA.data(),
          &info,
          DIB_RGB_COLORS) == 0) {
    output = {};
  }

  ReleaseDC(nullptr, hdc);
#endif
  return output;
}

}  // namespace atlas::core
