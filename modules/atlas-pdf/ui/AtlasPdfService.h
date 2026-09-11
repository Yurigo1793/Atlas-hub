#pragma once

#include "AtlasPdfDocument.h"

#include <QColor>
#include <QRectF>
#include <QString>

class AtlasPdfService
{
public:
    AtlasPdfDocument openDocument(const QString &filePath) const;
    AtlasPdfDocument::EditOperation createTextOperation(int pageNumber,
                                                        const QRectF &bounds,
                                                        const QString &text,
                                                        const QColor &color = Qt::black) const;
    AtlasPdfDocument::EditOperation createHighlightOperation(int pageNumber,
                                                            const QRectF &bounds,
                                                            const QColor &color = QColor(255, 235, 59, 120)) const;
    AtlasPdfDocument::EditOperation createSignatureOperation(int pageNumber,
                                                            const QRectF &bounds,
                                                            const QString &signatureText) const;
    AtlasPdfDocument::EditOperation createRotationOperation(int pageNumber,
                                                           qreal rotationDegrees) const;
    bool saveCopy(const AtlasPdfDocument &document,
                  const QString &destinationPath,
                  QString *errorMessage = nullptr) const;
    bool saveEditedCopy(const AtlasPdfDocument &document,
                        const QString &destinationPath,
                        QString *errorMessage = nullptr) const;

private:
    bool hasPdfHeader(const QString &filePath) const;
};
