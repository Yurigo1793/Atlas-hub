#include "AtlasPdfService.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QMarginsF>
#include <QPageSize>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>

AtlasPdfDocument AtlasPdfService::openDocument(const QString &filePath) const
{
    AtlasPdfDocument document(filePath);
    const QFileInfo info(filePath);

    if (!info.exists() || !info.isFile()) {
        document.setErrorMessage(QCoreApplication::translate("AtlasPdfService",
                                                             "PDF não encontrado: %1")
                                     .arg(filePath));
        return document;
    }

    if (!info.isReadable()) {
        document.setErrorMessage(QCoreApplication::translate("AtlasPdfService",
                                                             "PDF sem permissão de leitura: %1")
                                     .arg(filePath));
        return document;
    }

    if (info.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) != 0
        || !hasPdfHeader(filePath)) {
        document.setErrorMessage(QCoreApplication::translate("AtlasPdfService",
                                                             "Arquivo inválido ou não reconhecido como PDF: %1")
                                     .arg(filePath));
        return document;
    }

    return document;
}

AtlasPdfDocument::EditOperation AtlasPdfService::createTextOperation(int pageNumber,
                                                                     const QRectF &bounds,
                                                                     const QString &text,
                                                                     const QColor &color) const
{
    AtlasPdfDocument::EditOperation operation;
    operation.type = AtlasPdfDocument::EditOperationType::AddText;
    operation.pageNumber = pageNumber;
    operation.bounds = bounds;
    operation.text = text;
    operation.color = color;
    return operation;
}

AtlasPdfDocument::EditOperation AtlasPdfService::createHighlightOperation(int pageNumber,
                                                                          const QRectF &bounds,
                                                                          const QColor &color) const
{
    AtlasPdfDocument::EditOperation operation;
    operation.type = AtlasPdfDocument::EditOperationType::Highlight;
    operation.pageNumber = pageNumber;
    operation.bounds = bounds;
    operation.color = color;
    return operation;
}

AtlasPdfDocument::EditOperation AtlasPdfService::createSignatureOperation(int pageNumber,
                                                                          const QRectF &bounds,
                                                                          const QString &signatureText) const
{
    AtlasPdfDocument::EditOperation operation;
    operation.type = AtlasPdfDocument::EditOperationType::Signature;
    operation.pageNumber = pageNumber;
    operation.bounds = bounds;
    operation.text = signatureText;
    operation.color = Qt::black;
    return operation;
}

AtlasPdfDocument::EditOperation AtlasPdfService::createRotationOperation(int pageNumber,
                                                                         qreal rotationDegrees) const
{
    AtlasPdfDocument::EditOperation operation;
    operation.type = AtlasPdfDocument::EditOperationType::RotatePage;
    operation.pageNumber = pageNumber;
    operation.rotationDegrees = rotationDegrees;
    return operation;
}

bool AtlasPdfService::saveCopy(const AtlasPdfDocument &document,
                               const QString &destinationPath,
                               QString *errorMessage) const
{
    if (!document.isValid()) {
        if (errorMessage) {
            *errorMessage = document.errorMessage().isEmpty()
                                ? QCoreApplication::translate("AtlasPdfService", "Documento PDF inválido.")
                                : document.errorMessage();
        }
        return false;
    }

    if (QFile::exists(destinationPath) && !QFile::remove(destinationPath)) {
        if (errorMessage) {
            *errorMessage = QCoreApplication::translate("AtlasPdfService",
                                                        "Não foi possível substituir o PDF de destino: %1")
                                .arg(destinationPath);
        }
        return false;
    }

    if (!QFile::copy(document.filePath(), destinationPath)) {
        if (errorMessage) {
            *errorMessage = QCoreApplication::translate("AtlasPdfService",
                                                        "Não foi possível salvar a cópia do PDF em: %1")
                                .arg(destinationPath);
        }
        return false;
    }

    return true;
}

bool AtlasPdfService::saveEditedCopy(const AtlasPdfDocument &document,
                                     const QString &destinationPath,
                                     QString *errorMessage) const
{
    if (!document.hasPendingEdits()) {
        return saveCopy(document, destinationPath, errorMessage);
    }

    QPdfDocument pdfDocument;
    const QPdfDocument::Error loadError = pdfDocument.load(document.filePath());
    if (loadError != QPdfDocument::Error::None || pdfDocument.pageCount() <= 0) {
        if (errorMessage) {
            *errorMessage = QCoreApplication::translate(
                "AtlasPdfService",
                "Não foi possível carregar o PDF para aplicar as edições.");
        }
        return false;
    }

    QPdfWriter writer(destinationPath);
    writer.setResolution(144);
    writer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Point);

    QPainter painter;
    if (!painter.begin(&writer)) {
        if (errorMessage) {
            *errorMessage = QCoreApplication::translate("AtlasPdfService",
                                                        "Não foi possível iniciar a gravação do PDF: %1")
                                .arg(destinationPath);
        }
        return false;
    }

    const QVector<AtlasPdfDocument::EditOperation> operations = document.editOperations();
    for (int pageIndex = 0; pageIndex < pdfDocument.pageCount(); ++pageIndex) {
        if (pageIndex > 0) {
            writer.newPage();
        }

        const QSizeF pageSizePoints = pdfDocument.pagePointSize(pageIndex);
        writer.setPageSize(QPageSize(pageSizePoints, QPageSize::Point));

        const QSize imageSize(qMax(1, qRound(pageSizePoints.width() * 2.0)),
                              qMax(1, qRound(pageSizePoints.height() * 2.0)));
        const QImage pageImage = pdfDocument.render(pageIndex, imageSize);
        painter.drawImage(QRectF(QPointF(0, 0), pageSizePoints), pageImage);

        for (const AtlasPdfDocument::EditOperation &operation : operations) {
            if (operation.pageNumber != pageIndex + 1) {
                continue;
            }

            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);

            switch (operation.type) {
            case AtlasPdfDocument::EditOperationType::AddText: {
                QFont font = painter.font();
                font.setPointSizeF(qMax<qreal>(8.0, operation.bounds.height() * 0.55));
                painter.setFont(font);
                painter.setPen(operation.color);
                painter.drawText(operation.bounds, Qt::AlignLeft | Qt::AlignVCenter, operation.text);
                break;
            }
            case AtlasPdfDocument::EditOperationType::Highlight:
                painter.setPen(Qt::NoPen);
                painter.setBrush(operation.color);
                painter.drawRect(operation.bounds);
                break;
            case AtlasPdfDocument::EditOperationType::Signature: {
                QFont font = painter.font();
                font.setItalic(true);
                font.setPointSizeF(qMax<qreal>(10.0, operation.bounds.height() * 0.45));
                painter.setFont(font);
                painter.setPen(operation.color);
                painter.drawText(operation.bounds, Qt::AlignLeft | Qt::AlignVCenter, operation.text);
                painter.drawLine(operation.bounds.bottomLeft(), operation.bounds.bottomRight());
                break;
            }
            case AtlasPdfDocument::EditOperationType::RotatePage:
                break;
            }

            painter.restore();
        }
    }

    painter.end();
    return true;
}

bool AtlasPdfService::hasPdfHeader(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    return file.read(5) == QByteArrayLiteral("%PDF-");
}
