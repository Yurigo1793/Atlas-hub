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
CapturedImage capturarTelaInterna(const RECT& regiao) {
  CapturedImage image;
  const int largura = regiao.right - regiao.left;
  const int altura = regiao.bottom - regiao.top;

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
        regiao.left,
        regiao.top,
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
  RECT tela{};
  tela.left = 0;
  tela.top = 0;
  tela.right = GetSystemMetrics(SM_CXSCREEN);
  tela.bottom = GetSystemMetrics(SM_CYSCREEN);
  return capturarTelaInterna(tela);
#else
  return {};
#endif
}

#ifdef _WIN32
CapturedImage capturarTela(const RECT& regiao) {
  return capturarTelaInterna(regiao);
}
#endif

}  // namespace atlas::core
