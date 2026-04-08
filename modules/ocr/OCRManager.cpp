#include "OCRManager.h"

#include <QImage>

#if defined(ATLASHUB_HAS_TESSERACT)
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#endif

#include "utils/Logger.h"

namespace
{
constexpr auto kTessdataPath = "third_party/tesseract/tessdata";
constexpr auto kDefaultLanguage = "eng";

const QString kInvalidImageMessage = QStringLiteral("Invalid image");
const QString kOcrFailedMessage = QStringLiteral("OCR failed");
const QString kNoTextDetectedMessage = QStringLiteral("No text detected");
}

QString OCRManager::processImage(const QImage &image)
{
    Logger::instance().info(QStringLiteral("OCR started"));

#if !defined(ATLASHUB_HAS_TESSERACT)
    const QString noOcrEngineMessage = QStringLiteral("OCR engine not available (Tesseract not found at build time)");
    Logger::instance().warning(noOcrEngineMessage);
    return noOcrEngineMessage;
#else
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        Logger::instance().warning(kInvalidImageMessage);
        return kInvalidImageMessage;
    }

    QImage img = image.convertToFormat(QImage::Format_RGBA8888);
    Logger::instance().info(QStringLiteral("Image size: %1x%2").arg(img.width()).arg(img.height()));

    tesseract::TessBaseAPI tess;

    if (tess.Init(kTessdataPath, kDefaultLanguage) != 0) {
        const QString initError = QStringLiteral("Failed to initialize Tesseract (check tessdata path)");
        Logger::instance().error(initError);
        tess.End();
        return initError;
    }

    tess.SetImage(img.bits(), img.width(), img.height(), 4, img.bytesPerLine());

    char *outText = tess.GetUTF8Text();
    if (outText == nullptr) {
        tess.End();
        Logger::instance().error(kOcrFailedMessage);
        return kOcrFailedMessage;
    }

    QString result = QString::fromUtf8(outText).trimmed();
    Logger::instance().info(QStringLiteral("OCR result length: %1").arg(result.size()));

    delete[] outText;
    tess.End();

    if (result.isEmpty()) {
        Logger::instance().warning(kNoTextDetectedMessage);
        return kNoTextDetectedMessage;
    }

    return result;
#endif
}
