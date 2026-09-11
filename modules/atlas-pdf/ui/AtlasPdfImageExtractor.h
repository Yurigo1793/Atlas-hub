#pragma once

#include <QImage>
#include <QRectF>
#include <QString>
#include <QVector>

class AtlasPdfImageExtractor
{
public:
    struct PdfImage {
        int pageIndex = -1;
        QString name;
        QRectF pageRect;
        QImage image;
    };

    explicit AtlasPdfImageExtractor(QString filePath);

    QVector<PdfImage> pageImages(int pageIndex) const;
    bool hasAnyPageImage(int pageCount) const;
    int pageRotationDegrees(int pageIndex) const;

private:
    QString m_filePath;
};
