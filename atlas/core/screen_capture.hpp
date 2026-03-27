#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

namespace atlas::core {

struct CaptureRegion {
  int left = 0;
  int top = 0;
  int width = 0;
  int height = 0;
};

struct CapturedImage {
#ifdef _WIN32
  HBITMAP bitmap = nullptr;
#endif
  int width = 0;
  int height = 0;

  void release();
};

CapturedImage capturarTela();
CapturedImage capturarTela(const CaptureRegion& regiao);

}  // namespace atlas::core
