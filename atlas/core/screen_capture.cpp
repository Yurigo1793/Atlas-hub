#include "core/screen_capture.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace atlas::core {

void CapturedImage::release() {
#ifdef _WIN32
  if (bitmap != nullptr) {
    DeleteObject(bitmap);
    bitmap = nullptr;
  }
#endif
  width = 0;
  height = 0;
}

namespace {
#ifdef _WIN32
CapturedImage capturarTelaInterna(int left, int top, int largura, int altura) {
  CapturedImage image;

  HDC hdcTela = GetDC(nullptr);
  if (hdcTela == nullptr) {
    return image;
  }

  HDC hdcMemoria = CreateCompatibleDC(hdcTela);
  HBITMAP hBitmap = CreateCompatibleBitmap(hdcTela, largura, altura);

  if (hdcMemoria != nullptr && hBitmap != nullptr) {
    SelectObject(hdcMemoria, hBitmap);
    BitBlt(
        hdcMemoria,
        0,
        0,
        largura,
        altura,
        hdcTela,
        left,
        top,
        SRCCOPY | CAPTUREBLT);

    image.bitmap = hBitmap;
    image.width = largura;
    image.height = altura;
  } else if (hBitmap != nullptr) {
    DeleteObject(hBitmap);
  }

  if (hdcMemoria != nullptr) {
    DeleteDC(hdcMemoria);
  }
  ReleaseDC(nullptr, hdcTela);

  return image;
}
#endif
}  // namespace

CapturedImage capturarTela() {
#ifdef _WIN32
  const int largura = GetSystemMetrics(SM_CXSCREEN);
  const int altura = GetSystemMetrics(SM_CYSCREEN);
  return capturarTelaInterna(0, 0, largura, altura);
#else
  return {};
#endif
}

CapturedImage capturarTela(const CaptureRegion& regiao) {
#ifdef _WIN32
  if (regiao.width <= 0 || regiao.height <= 0) {
    return {};
  }
  return capturarTelaInterna(regiao.left, regiao.top, regiao.width, regiao.height);
#else
  (void)regiao;
  return {};
#endif
}

}  // namespace atlas::core
