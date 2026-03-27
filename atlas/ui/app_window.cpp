#include "ui/app_window.hpp"

#include <array>
#include <optional>
#include <utility>

#ifdef _WIN32
#include <windowsx.h>
#endif

namespace atlas::ui {

namespace {
#ifdef _WIN32
constexpr wchar_t kWindowClass[] = L"AtlasHubWindowClass";
constexpr wchar_t kSelectionClass[] = L"AtlasHubSelectionClass";
constexpr COLORREF kAzulClaro = RGB(0x8F, 0xAF, 0xCF);
constexpr COLORREF kAzulMedio = RGB(0x5F, 0x7F, 0x9F);
constexpr COLORREF kAzulEscuro = RGB(0x2F, 0x4F, 0x6F);

struct SelectionState {
  int originX = 0;
  int originY = 0;
  bool dragging = false;
  bool confirmed = false;
  POINT start{};
  POINT current{};
  atlas::core::CaptureRegion region{};
};

enum ButtonId : int {
  kBtnOcrTela = 1001,
  kBtnConfig = 1002,
  kBtnSair = 1003,
  kTxtResultado = 2001,
};

constexpr std::array kFontCatalog = {
    L"Press Start 2P",     L"Pixel Operator",   L"Pixelmix",          L"Silkscreen",
    L"8 Bit Wonder",       L"Terminus",         L"PxPlus IBM VGA9",   L"Perfect DOS VGA",
    L"Minecraft",          L"Upheaval",         L"04b_03",            L"04b_30",
    L"Visitor TT1 BRK",    L"Fixedsys Excelsior", L"Proggy Clean",    L"Spleen",
    L"Cozette",            L"Tamzen",           L"Gohu",              L"Unifont",
};

int ptToPixels(int pointSize) {
  HDC hdc = GetDC(nullptr);
  const int logPixels = GetDeviceCaps(hdc, LOGPIXELSY);
  ReleaseDC(nullptr, hdc);
  return -MulDiv(pointSize, logPixels, 72);
}

LRESULT CALLBACK SelectionWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  auto* state = reinterpret_cast<SelectionState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  switch (msg) {
    case WM_NCCREATE: {
      auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
      auto* initialState = reinterpret_cast<SelectionState*>(createStruct->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(initialState));
      return TRUE;
    }
    case WM_SETCURSOR:
      SetCursor(LoadCursor(nullptr, IDC_CROSS));
      return TRUE;
    case WM_LBUTTONDOWN: {
      if (state == nullptr) return 0;
      state->dragging = true;
      state->start.x = GET_X_LPARAM(lParam);
      state->start.y = GET_Y_LPARAM(lParam);
      state->current = state->start;
      SetCapture(hwnd);
      InvalidateRect(hwnd, nullptr, TRUE);
      return 0;
    }
    case WM_MOUSEMOVE: {
      if (state == nullptr || !state->dragging) return 0;
      state->current.x = GET_X_LPARAM(lParam);
      state->current.y = GET_Y_LPARAM(lParam);
      InvalidateRect(hwnd, nullptr, TRUE);
      return 0;
    }
    case WM_LBUTTONUP: {
      if (state == nullptr) return 0;
      if (state->dragging) {
        state->dragging = false;
        ReleaseCapture();

        RECT normalized{};
        normalized.left = min(state->start.x, GET_X_LPARAM(lParam));
        normalized.top = min(state->start.y, GET_Y_LPARAM(lParam));
        normalized.right = max(state->start.x, GET_X_LPARAM(lParam));
        normalized.bottom = max(state->start.y, GET_Y_LPARAM(lParam));

        const int width = normalized.right - normalized.left;
        const int height = normalized.bottom - normalized.top;

        if (width > 2 && height > 2) {
          state->region.left = state->originX + normalized.left;
          state->region.top = state->originY + normalized.top;
          state->region.width = width;
          state->region.height = height;
          state->confirmed = true;
        }
      }
      DestroyWindow(hwnd);
      return 0;
    }
    case WM_KEYDOWN:
      if (wParam == VK_ESCAPE) {
        DestroyWindow(hwnd);
        return 0;
      }
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT ps{};
      HDC hdc = BeginPaint(hwnd, &ps);

      RECT client{};
      GetClientRect(hwnd, &client);
      FillRect(hdc, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

      if (state != nullptr && state->dragging) {
        RECT selection{};
        selection.left = min(state->start.x, state->current.x);
        selection.top = min(state->start.y, state->current.y);
        selection.right = max(state->start.x, state->current.x);
        selection.bottom = max(state->start.y, state->current.y);

        HPEN pen = CreatePen(PS_SOLID, 2, RGB(143, 175, 207));
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(hdc, selection.left, selection.top, selection.right, selection.bottom);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
      }

      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_DESTROY:
      return 0;
    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}
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
  wc.hbrBackground = CreateSolidBrush(uiSettings_.panelColor);
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
      720,
      460,
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
  if (outputText_ == nullptr) return;

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

  if (self != nullptr) return self->handleMessage(hwnd, msg, wParam, lParam);
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT AppWindow::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE: {
      ocrButton_ = CreateWindowExW(
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

      configButton_ = CreateWindowExW(
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

      sairButton_ = CreateWindowExW(
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

      organizarLayout(hwnd);
      aplicarTemaVisual(hwnd);
      hotkeyManager_.registrarAtalhosGlobais(hwnd_);
      return 0;
    }

    case WM_SIZE:
      organizarLayout(hwnd);
      return 0;

    case WM_COMMAND: {
      const int controlId = LOWORD(wParam);
      switch (controlId) {
        case kBtnOcrTela:
          if (onOcrRequest_) atualizarResultado(onOcrRequest_(std::nullopt));
          return 0;
        case kBtnConfig:
          aplicarProximaConfiguracaoVisual();
          atualizarResultado(descreverConfiguracaoAtual());
          return 0;
        case kBtnSair:
          PostQuitMessage(0);
          return 0;
        default:
          return DefWindowProcW(hwnd, msg, wParam, lParam);
      }
    }

    case WM_HOTKEY:
      if (wParam == atlas::input::HotkeyManager::kAbrirInterface) {
        ShowWindow(hwnd_, SW_SHOW);
        SetForegroundWindow(hwnd_);
      } else if (wParam == atlas::input::HotkeyManager::kExecutarOcr) {
        executarOcrPorSelecao();
      }
      return 0;

    case WM_DRAWITEM:
      desenharBotao(reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));
      return TRUE;

    case WM_DESTROY:
      if (uiFont_ != nullptr) {
        DeleteObject(uiFont_);
        uiFont_ = nullptr;
      }
      hotkeyManager_.removerAtalhosGlobais(hwnd_);
      PostQuitMessage(0);
      return 0;

    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}

void AppWindow::organizarLayout(HWND hwnd) {
  RECT client{};
  GetClientRect(hwnd, &client);

  constexpr int margin = 16;
  constexpr int spacing = 10;
  constexpr int buttonWidth = 130;
  constexpr int buttonHeight = 42;

  if (ocrButton_ != nullptr) MoveWindow(ocrButton_, margin, margin, buttonWidth, buttonHeight, TRUE);
  if (configButton_ != nullptr)
    MoveWindow(configButton_, margin + buttonWidth + spacing, margin, buttonWidth, buttonHeight, TRUE);
  if (sairButton_ != nullptr)
    MoveWindow(sairButton_, margin + (buttonWidth + spacing) * 2, margin, buttonWidth, buttonHeight, TRUE);

  if (outputText_ != nullptr) {
    const int outputTop = margin + buttonHeight + 14;
    MoveWindow(
        outputText_,
        margin,
        outputTop,
        (client.right - client.left) - (margin * 2),
        (client.bottom - client.top) - outputTop - margin,
        TRUE);
  }
}

void AppWindow::aplicarProximaConfiguracaoVisual() {
  static size_t fontIndex = 0;
  static size_t sizeIndex = 0;
  static size_t opacityIndex = 0;
  static size_t colorIndex = 0;
  static bool bold = true;

  constexpr std::array<int, 4> kFontSizes = {10, 11, 12, 14};
  constexpr std::array<BYTE, 4> kOpacities = {220, 235, 245, 255};
  constexpr std::array<COLORREF, 4> kTextColors = {
      RGB(255, 255, 255), RGB(220, 235, 255), RGB(200, 220, 240), RGB(240, 250, 255)};

  fontIndex = (fontIndex + 1) % kFontCatalog.size();
  sizeIndex = (sizeIndex + 1) % kFontSizes.size();
  opacityIndex = (opacityIndex + 1) % kOpacities.size();
  colorIndex = (colorIndex + 1) % kTextColors.size();
  bold = !bold;

  uiSettings_.fontName = kFontCatalog[fontIndex];
  uiSettings_.fontSizePt = kFontSizes[sizeIndex];
  uiSettings_.windowOpacity = kOpacities[opacityIndex];
  uiSettings_.fontColor = kTextColors[colorIndex];

  if (uiFont_ != nullptr) {
    DeleteObject(uiFont_);
    uiFont_ = nullptr;
  }

  uiFont_ = CreateFontW(
      ptToPixels(uiSettings_.fontSizePt),
      0,
      0,
      0,
      bold ? FW_BOLD : FW_NORMAL,
      FALSE,
      FALSE,
      FALSE,
      DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS,
      CLIP_DEFAULT_PRECIS,
      CLEARTYPE_QUALITY,
      FF_DONTCARE,
      uiSettings_.fontName.c_str());

  if (uiFont_ != nullptr) {
    if (ocrButton_ != nullptr) SendMessageW(ocrButton_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
    if (configButton_ != nullptr)
      SendMessageW(configButton_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
    if (sairButton_ != nullptr) SendMessageW(sairButton_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
    if (outputText_ != nullptr) SendMessageW(outputText_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
  }

  SetLayeredWindowAttributes(hwnd_, 0, uiSettings_.windowOpacity, LWA_ALPHA);
  InvalidateRect(hwnd_, nullptr, TRUE);
}

std::string AppWindow::descreverConfiguracaoAtual() const {
  std::string fontName(uiSettings_.fontName.begin(), uiSettings_.fontName.end());
  return "Config aplicada -> fonte: " + fontName +
         ", tamanho: " + std::to_string(uiSettings_.fontSizePt) +
         "pt, transparencia: " + std::to_string(static_cast<int>(uiSettings_.windowOpacity)) + "/255";
}

void AppWindow::aplicarTemaVisual(HWND hwnd) {
  SetWindowLongPtrW(hwnd, GWL_EXSTYLE, GetWindowLongPtrW(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
  SetLayeredWindowAttributes(hwnd, 0, uiSettings_.windowOpacity, LWA_ALPHA);

  if (uiFont_ != nullptr) {
    DeleteObject(uiFont_);
    uiFont_ = nullptr;
  }

  uiFont_ = CreateFontW(
      ptToPixels(uiSettings_.fontSizePt),
      0,
      0,
      0,
      FW_BOLD,
      FALSE,
      FALSE,
      FALSE,
      DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS,
      CLIP_DEFAULT_PRECIS,
      CLEARTYPE_QUALITY,
      FF_DONTCARE,
      uiSettings_.fontName.c_str());

  if (uiFont_ != nullptr) {
    if (ocrButton_ != nullptr) SendMessageW(ocrButton_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
    if (configButton_ != nullptr)
      SendMessageW(configButton_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
    if (sairButton_ != nullptr) SendMessageW(sairButton_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
    if (outputText_ != nullptr) SendMessageW(outputText_, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
  }
}

std::optional<atlas::core::CaptureRegion> AppWindow::selecionarRegiaoTela() {
  WNDCLASSW wc{};
  wc.lpfnWndProc = SelectionWindowProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = kSelectionClass;
  wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
  wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  RegisterClassW(&wc);

  SelectionState state{};
  state.originX = GetSystemMetrics(SM_XVIRTUALSCREEN);
  state.originY = GetSystemMetrics(SM_YVIRTUALSCREEN);

  HWND overlay = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
      kSelectionClass,
      L"",
      WS_POPUP,
      state.originX,
      state.originY,
      GetSystemMetrics(SM_CXVIRTUALSCREEN),
      GetSystemMetrics(SM_CYVIRTUALSCREEN),
      nullptr,
      nullptr,
      GetModuleHandleW(nullptr),
      &state);

  if (overlay == nullptr) return std::nullopt;

  SetLayeredWindowAttributes(overlay, 0, 80, LWA_ALPHA);
  ShowWindow(overlay, SW_SHOW);
  SetForegroundWindow(overlay);
  SetFocus(overlay);

  MSG msg{};
  while (IsWindow(overlay) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  if (!state.confirmed) return std::nullopt;
  return state.region;
}

void AppWindow::executarOcrPorSelecao() {
  ShowWindow(hwnd_, SW_HIDE);
  Sleep(120);

  auto regiao = selecionarRegiaoTela();

  ShowWindow(hwnd_, SW_SHOW);
  SetForegroundWindow(hwnd_);

  if (!regiao.has_value()) {
    atualizarResultado("Selecao cancelada.");
    return;
  }

  if (onOcrRequest_) atualizarResultado(onOcrRequest_(regiao));
}

void AppWindow::desenharBotao(LPDRAWITEMSTRUCT drawInfo) {
  if (drawInfo == nullptr) return;

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
  SetTextColor(drawInfo->hDC, uiSettings_.fontColor);

  wchar_t label[64] = {};
  GetWindowTextW(drawInfo->hwndItem, label, 64);
  DrawTextW(drawInfo->hDC, label, -1, &drawInfo->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}
#endif

}  // namespace atlas::ui
