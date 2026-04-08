#include "OCRManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QImage>

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#include "utils/Logger.h"

namespace
{
constexpr auto kDefaultLanguage = "eng";

const QString kInvalidImageMessage = QStringLiteral("Invalid image");
const QString kOcrFailedMessage = QStringLiteral("OCR failed");
const QString kNoTextDetectedMessage = QStringLiteral("No text detected");
}

QString OCRManager::processImage(const QImage &image)
{
    Logger::instance().info(QStringLiteral("OCR started"));

    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        Logger::instance().warning(kInvalidImageMessage);
        return kInvalidImageMessage;
    }

    QImage img = image.convertToFormat(QImage::Format_RGBA8888);
    Logger::instance().info(QStringLiteral("Image size: %1x%2").arg(img.width()).arg(img.height()));

    tesseract::TessBaseAPI tess;

    const QString tessdataPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("tessdata"));

    if (tess.Init(tessdataPath.toUtf8().constData(), kDefaultLanguage) != 0) {
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
}
