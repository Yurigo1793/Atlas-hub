#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

namespace atlas::core {

struct CapturedImage {
#ifdef _WIN32
  HBITMAP bitmap = nullptr;
#endif
  int width = 0;
  int height = 0;

  void release();
};

CapturedImage capturarTela();
#ifdef _WIN32
CapturedImage capturarTela(const RECT& regiao);
#endif

}  // namespace atlas::core
