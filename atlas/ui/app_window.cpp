#include "ui/app_window.hpp"

#include <utility>

#ifdef _WIN32
#include <windowsx.h>
#endif

namespace atlas::ui {

namespace {
#ifdef _WIN32
constexpr wchar_t kWindowClass[] = L"AtlasHubWindowClass";
constexpr COLORREF kAzulClaro = RGB(0x8F, 0xAF, 0xCF);
constexpr COLORREF kAzulMedio = RGB(0x5F, 0x7F, 0x9F);
constexpr COLORREF kAzulEscuro = RGB(0x2F, 0x4F, 0x6F);

enum ButtonId : int {
  kBtnOcrTela = 1001,
  kBtnConfig = 1002,
  kBtnSair = 1003,
  kTxtResultado = 2001,
};
#endif
}  // namespace

AppWindow::AppWindow(OcrCallback onOcrRequest) : onOcrRequest_(std::move(onOcrRequest)) {}

bool AppWindow::criar(void* instanceHandle) {
#ifdef _WIN32
  WNDCLASSW wc{};
  wc.lpfnWndProc = AppWindow::WindowProc;
  HINSTANCE instance = reinterpret_cast<HINSTANCE>(instanceHandle);
  wc.hInstance = instance;
  wc.lpszClassName = kWindowClass;
  wc.hbrBackground = CreateSolidBrush(kAzulEscuro);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

  if (RegisterClassW(&wc) == 0) {
    return false;
  }

  hwnd_ = CreateWindowExW(
      0,
      kWindowClass,
      L"Atlas-Hub",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      640,
      420,
      nullptr,
      nullptr,
      instance,
      this);

  return hwnd_ != nullptr;
#else
  (void)instanceHandle;
  return false;
#endif
}

int AppWindow::executarLoop() {
#ifdef _WIN32
  ShowWindow(hwnd_, SW_SHOW);

  MSG msg{};
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
#else
  return 0;
#endif
}

void AppWindow::atualizarResultado(const std::string& texto) {
#ifdef _WIN32
  if (outputText_ == nullptr) {
    return;
  }

  std::wstring textoWide(texto.begin(), texto.end());
  SetWindowTextW(outputText_, textoWide.c_str());
#else
  (void)texto;
#endif
}

#ifdef _WIN32
LRESULT CALLBACK AppWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  AppWindow* self = nullptr;

  if (msg == WM_NCCREATE) {
    auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = reinterpret_cast<AppWindow*>(createStruct->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }

  if (self != nullptr) {
    return self->handleMessage(hwnd, msg, wParam, lParam);
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT AppWindow::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE: {
      CreateWindowExW(
          0,
          L"BUTTON",
          L"OCR TELA",
          WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
          20,
          20,
          120,
          40,
          hwnd,
          reinterpret_cast<HMENU>(kBtnOcrTela),
          GetModuleHandleW(nullptr),
          nullptr);

      CreateWindowExW(
          0,
          L"BUTTON",
          L"CONFIG",
          WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
          150,
          20,
          120,
          40,
          hwnd,
          reinterpret_cast<HMENU>(kBtnConfig),
          GetModuleHandleW(nullptr),
          nullptr);

      CreateWindowExW(
          0,
          L"BUTTON",
          L"SAIR",
          WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
          280,
          20,
          120,
          40,
          hwnd,
          reinterpret_cast<HMENU>(kBtnSair),
          GetModuleHandleW(nullptr),
          nullptr);

      outputText_ = CreateWindowExW(
          WS_EX_CLIENTEDGE,
          L"EDIT",
          L"Resultados do OCR aparecerao aqui...",
          WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
          20,
          80,
          580,
          280,
          hwnd,
          reinterpret_cast<HMENU>(kTxtResultado),
          GetModuleHandleW(nullptr),
          nullptr);
      return 0;
    }

    case WM_COMMAND: {
      const int controlId = LOWORD(wParam);
      switch (controlId) {
        case kBtnOcrTela:
          if (onOcrRequest_) {
            atualizarResultado(onOcrRequest_());
          }
          return 0;
        case kBtnConfig:
          atualizarResultado("Configuracoes serao adicionadas em uma futura versao modular.");
          return 0;
        case kBtnSair:
          PostQuitMessage(0);
          return 0;
        default:
          return DefWindowProcW(hwnd, msg, wParam, lParam);
      }
    }

    case WM_DRAWITEM:
      desenharBotao(reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));
      return TRUE;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}

void AppWindow::desenharBotao(LPDRAWITEMSTRUCT drawInfo) {
  if (drawInfo == nullptr) {
    return;
  }

  const bool pressionado = (drawInfo->itemState & ODS_SELECTED) != 0;
  const bool hover = (drawInfo->itemState & ODS_HOTLIGHT) != 0;

  COLORREF fundo = kAzulMedio;
  if (pressionado) {
    fundo = kAzulEscuro;
  } else if (hover) {
    fundo = kAzulClaro;
  }

  HBRUSH brush = CreateSolidBrush(fundo);
  FillRect(drawInfo->hDC, &drawInfo->rcItem, brush);
  DeleteObject(brush);

  HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
  HGDIOBJ oldPen = SelectObject(drawInfo->hDC, borderPen);
  HGDIOBJ oldBrush = SelectObject(drawInfo->hDC, GetStockObject(HOLLOW_BRUSH));
  Rectangle(
      drawInfo->hDC,
      drawInfo->rcItem.left,
      drawInfo->rcItem.top,
      drawInfo->rcItem.right,
      drawInfo->rcItem.bottom);
  SelectObject(drawInfo->hDC, oldBrush);
  SelectObject(drawInfo->hDC, oldPen);
  DeleteObject(borderPen);

  SetBkMode(drawInfo->hDC, TRANSPARENT);
  SetTextColor(drawInfo->hDC, RGB(255, 255, 255));

  wchar_t label[64] = {};
  GetWindowTextW(drawInfo->hwndItem, label, 64);
  DrawTextW(drawInfo->hDC, label, -1, &drawInfo->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}
#endif

}  // namespace atlas::ui
