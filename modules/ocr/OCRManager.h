#pragma once

#include <QString>

class QImage;

/**
 * @brief OCR module boundary.
 *
 * This class abstracts OCR providers and keeps the rest of the application
 * isolated from provider-specific details.
 */
class OCRManager
{
public:
    OCRManager() = default;

    QString processImage(const QImage &image);

private:
    QString resolveTessdataPath() const;
};
