#include "core/ocr_engine.hpp"

#include <sstream>

#include "core/image_processing.hpp"

#if defined(_WIN32) && __has_include(<winrt/Windows.Foundation.h>)
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>
#endif

namespace atlas::core {

std::string OcrEngine::reconhecerTexto(const CapturedImage& image) const {
#if defined(_WIN32) && __has_include(<winrt/Windows.Foundation.h>)
  if (image.bitmap == nullptr) {
    return "Falha: captura de tela vazia.";
  }

  auto pixels = converterBitmapParaPixels(image);
  if (pixels.pixelsBGRA.empty()) {
    return "Falha: nao foi possivel converter a captura para pixels.";
  }

  winrt::init_apartment(winrt::apartment_type::multi_threaded);

  using namespace winrt::Windows::Graphics::Imaging;
  using namespace winrt::Windows::Media::Ocr;
  using namespace winrt::Windows::Storage::Streams;

  Buffer buffer(static_cast<uint32_t>(pixels.pixelsBGRA.size()));
  buffer.Length(static_cast<uint32_t>(pixels.pixelsBGRA.size()));

  auto writer = DataWriter();
  writer.WriteBytes(winrt::array_view<const uint8_t>(
      pixels.pixelsBGRA.data(), pixels.pixelsBGRA.data() + pixels.pixelsBGRA.size()));
  buffer = writer.DetachBuffer();

  SoftwareBitmap softwareBitmap = SoftwareBitmap::CreateCopyFromBuffer(
      buffer,
      BitmapPixelFormat::Bgra8,
      pixels.width,
      pixels.height,
      BitmapAlphaMode::Premultiplied,
      pixels.stride);

  auto ocrEngine = winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromUserProfileLanguages();
  if (ocrEngine == nullptr) {
    return "Falha: OCR indisponivel para os idiomas do perfil atual.";
  }

  OcrResult result = ocrEngine.RecognizeAsync(softwareBitmap).get();
  std::wstring textoWide = result.Text().c_str();
  std::string texto(textoWide.begin(), textoWide.end());

  if (texto.empty()) {
    return "OCR executado, mas nenhum texto foi reconhecido.";
  }

  return texto;
#else
  (void)image;
  return "OCR nativo do Windows requer compilacao em ambiente Windows com WinRT.";
#endif
}

}  // namespace atlas::core
