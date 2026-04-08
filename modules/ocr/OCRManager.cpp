#include "OCRManager.h"

#include <QImage>

#include <exception>

#include "utils/Logger.h"

#ifdef _WIN32
#if __has_include(<winrt/Windows.Foundation.h>) && __has_include(<winrt/Windows.Graphics.Imaging.h>) && \
    __has_include(<winrt/Windows.Media.Ocr.h>)
#define ATLASHUB_HAS_WINRT_OCR 1
#include <algorithm>
#include <cstring>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#else
#define ATLASHUB_HAS_WINRT_OCR 0
#endif
#endif

#if defined(_WIN32) && ATLASHUB_HAS_WINRT_OCR
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
    const QImage argbImage = image.convertToFormat(QImage::Format_ARGB32);

    const int width = argbImage.width();
    const int height = argbImage.height();

    SoftwareBitmap bitmap(BitmapPixelFormat::Bgra8, width, height, BitmapAlphaMode::Premultiplied);

    BitmapBuffer buffer = bitmap.LockBuffer(BitmapBufferAccessMode::Write);
    const BitmapPlaneDescription plane = buffer.GetPlaneDescription(0);
    auto reference = buffer.CreateReference();

    com_ptr<IMemoryBufferByteAccess> byteAccess = reference.as<IMemoryBufferByteAccess>();

    uint8_t *dstBytes = nullptr;
    uint32_t capacity = 0;
    check_hresult(byteAccess->GetBuffer(&dstBytes, &capacity));

    const int srcBytesPerLine = argbImage.bytesPerLine();
    const int dstBytesPerLine = plane.Stride;
    const int bytesToCopyPerLine = (std::min)(srcBytesPerLine, dstBytesPerLine);

    const uint8_t *srcBits = argbImage.constBits();
    uint8_t *dstStart = dstBytes + plane.StartIndex;

    for (int y = 0; y < height; ++y) {
        const uint8_t *srcLine = srcBits + (y * srcBytesPerLine);
        uint8_t *dstLine = dstStart + (y * dstBytesPerLine);
        std::memcpy(dstLine, srcLine, static_cast<size_t>(bytesToCopyPerLine));
    }

    Logger::instance().info(
        QStringLiteral("OCR bitmap creation complete: %1x%2, srcStride=%3, dstStride=%4")
            .arg(width)
            .arg(height)
            .arg(srcBytesPerLine)
            .arg(dstBytesPerLine));

    return bitmap;
}

QString runWindowsOcr(const QImage &image)
{
    try {
        init_apartment();
    } catch (const hresult_changed_mode &) {
        Logger::instance().info(QStringLiteral("WinRT apartment already initialized with different mode; continuing"));
    }

    const SoftwareBitmap bitmap = qImageToSoftwareBitmap(image);

    Logger::instance().info(QStringLiteral("Creating OCR engine from user profile languages"));
    const OcrEngine engine = OcrEngine::TryCreateFromUserProfileLanguages();
    if (!engine) {
        Logger::instance().error(QStringLiteral("OCR engine creation failed: TryCreateFromUserProfileLanguages returned null"));
        return QStringLiteral("OCR failed");
    }

    const OcrResult result = engine.RecognizeAsync(bitmap).get();
    const hstring text = result.Text();
    const QString extractedText = QString::fromWCharArray(text.c_str());

    Logger::instance().info(QStringLiteral("OCR result length: %1").arg(extractedText.size()));

    if (extractedText.isEmpty()) {
        Logger::instance().warning(QStringLiteral("OCR produced empty text"));
    }

    return extractedText;
}
}
#endif

QString OCRManager::processImage(const QImage &image) const
{
    if (image.isNull()) {
        Logger::instance().warning(QStringLiteral("OCR failed: input image is null"));
        return QStringLiteral("OCR failed");
    }

#if defined(_WIN32) && ATLASHUB_HAS_WINRT_OCR
    try {
        return runWindowsOcr(image);
    } catch (const winrt::hresult_error &err) {
        Logger::instance().error(QStringLiteral("OCR exception: %1").arg(QString::fromWCharArray(err.message().c_str())));
        return QStringLiteral("OCR failed");
    } catch (const std::exception &err) {
        Logger::instance().error(QStringLiteral("OCR exception: %1").arg(QString::fromUtf8(err.what())));
        return QStringLiteral("OCR failed");
    } catch (...) {
        Logger::instance().error(QStringLiteral("OCR exception: unknown error"));
        return QStringLiteral("OCR failed");
    }
#else
    Q_UNUSED(image);
    Logger::instance().warning(QStringLiteral("OCR failed: WinRT OCR headers are unavailable for this build"));
    return QStringLiteral("OCR failed");
#endif
}
