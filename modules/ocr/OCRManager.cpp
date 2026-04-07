#include "OCRManager.h"

#include <QImage>

#ifdef _WIN32
#include <algorithm>
#include <cstring>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>

using namespace winrt;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Media::Ocr;

namespace
{

struct __declspec(uuid("5B0D3235-4DBA-4D44-865E-8F1D0E4FD04D")) IMemoryBufferByteAccess : ::IUnknown
{
    virtual HRESULT __stdcall GetBuffer(uint8_t **value, uint32_t *capacity) = 0;
};

SoftwareBitmap qImageToSoftwareBitmap(const QImage &image)
{
    const QImage rgbaImage = image.convertToFormat(QImage::Format_RGBA8888);

    const int width = rgbaImage.width();
    const int height = rgbaImage.height();

    SoftwareBitmap bitmap(BitmapPixelFormat::Rgba8, width, height, BitmapAlphaMode::Ignore);

    BitmapBuffer buffer = bitmap.LockBuffer(BitmapBufferAccessMode::Write);
    auto reference = buffer.CreateReference();

    com_ptr<IMemoryBufferByteAccess> byteAccess = reference.as<IMemoryBufferByteAccess>();

    uint8_t *dstBytes = nullptr;
    uint32_t capacity = 0;
    check_hresult(byteAccess->GetBuffer(&dstBytes, &capacity));

    const int srcBytesPerLine = rgbaImage.bytesPerLine();
    const int dstBytesPerLine = width * 4;
    const int bytesToCopyPerLine = (std::min)(srcBytesPerLine, dstBytesPerLine);

    const uint8_t *srcBits = rgbaImage.constBits();
    for (int y = 0; y < height; ++y) {
        const uint8_t *srcLine = srcBits + (y * srcBytesPerLine);
        uint8_t *dstLine = dstBytes + (y * dstBytesPerLine);
        std::memcpy(dstLine, srcLine, static_cast<size_t>(bytesToCopyPerLine));
    }

    return bitmap;
}

QString runWindowsOcr(const QImage &image)
{
    init_apartment(apartment_type::multi_threaded);

    const SoftwareBitmap bitmap = qImageToSoftwareBitmap(image);
    const OcrEngine engine = OcrEngine::TryCreateFromUserProfileLanguages();
    if (!engine) {
        return QStringLiteral("OCR failed");
    }

    const OcrResult result = engine.RecognizeAsync(bitmap).get();
    const hstring text = result.Text();

    return QString::fromWCharArray(text.c_str());
}
}
#endif

QString OCRManager::processImage(const QImage &image) const
{
    if (image.isNull()) {
        return QStringLiteral("OCR failed");
    }

#ifdef _WIN32
    try {
        return runWindowsOcr(image);
    } catch (...) {
        return QStringLiteral("OCR failed");
    }
#else
    Q_UNUSED(image);
    return QStringLiteral("OCR failed");
#endif
}
