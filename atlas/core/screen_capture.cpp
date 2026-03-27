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

CapturedImage recortarImagem(const CapturedImage& origem, const CaptureRegion& regiaoRelativa) {
#ifdef _WIN32
  if (origem.bitmap == nullptr || origem.width <= 0 || origem.height <= 0) {
    return {};
  }

  if (regiaoRelativa.width <= 0 || regiaoRelativa.height <= 0) {
    return {};
  }

  if (regiaoRelativa.left < 0 || regiaoRelativa.top < 0 ||
      regiaoRelativa.left + regiaoRelativa.width > origem.width ||
      regiaoRelativa.top + regiaoRelativa.height > origem.height) {
    return {};
  }

  HDC hdcTela = GetDC(nullptr);
  if (hdcTela == nullptr) {
    return {};
  }

  HDC hdcOrigem = CreateCompatibleDC(hdcTela);
  HDC hdcDestino = CreateCompatibleDC(hdcTela);
  HBITMAP bitmapDestino = CreateCompatibleBitmap(hdcTela, regiaoRelativa.width, regiaoRelativa.height);

  CapturedImage recorte{};
  if (hdcOrigem != nullptr && hdcDestino != nullptr && bitmapDestino != nullptr) {
    HGDIOBJ oldOrigem = SelectObject(hdcOrigem, origem.bitmap);
    HGDIOBJ oldDestino = SelectObject(hdcDestino, bitmapDestino);
    BitBlt(
        hdcDestino,
        0,
        0,
        regiaoRelativa.width,
        regiaoRelativa.height,
        hdcOrigem,
        regiaoRelativa.left,
        regiaoRelativa.top,
        SRCCOPY);
    SelectObject(hdcDestino, oldDestino);
    SelectObject(hdcOrigem, oldOrigem);

    recorte.bitmap = bitmapDestino;
    recorte.width = regiaoRelativa.width;
    recorte.height = regiaoRelativa.height;
  } else if (bitmapDestino != nullptr) {
    DeleteObject(bitmapDestino);
  }

  if (hdcOrigem != nullptr) DeleteDC(hdcOrigem);
  if (hdcDestino != nullptr) DeleteDC(hdcDestino);
  ReleaseDC(nullptr, hdcTela);
  return recorte;
#else
  (void)origem;
  (void)regiaoRelativa;
  return {};
#endif
}

}  // namespace atlas::core
