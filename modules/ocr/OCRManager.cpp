#include "OCRManager.h"

#include <QImage>

QString OCRManager::processImage(const QImage &image) const
{
    Q_UNUSED(image);

    return QStringLiteral("OCR simulated text");
}
