#include "OCRManager.h"

#include <QDateTime>
#include <QImage>

QString OCRManager::extractText(const QImage &image) const
{
    Q_UNUSED(image);

    // Placeholder return until a concrete OCR engine is integrated.
    return QStringLiteral("[OCR Placeholder] Texto extraído com sucesso (simulado) em %1")
        .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
}
