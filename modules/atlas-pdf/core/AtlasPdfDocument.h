#pragma once

#include <QColor>
#include <QRectF>
#include <QString>
#include <QVector>

class AtlasPdfDocument
{
public:
    enum class EditOperationType
    {
        AddText,
        Highlight,
        Signature,
        RotatePage
    };

    struct EditOperation
    {
        EditOperationType type = EditOperationType::AddText;
        int pageNumber = 1;
        QRectF bounds;
        QString text;
        QColor color = Qt::black;
        qreal rotationDegrees = 0.0;
    };

    AtlasPdfDocument() = default;
    explicit AtlasPdfDocument(const QString &filePath);

    bool isValid() const;
    QString filePath() const;
    QString fileName() const;
    QString errorMessage() const;
    qint64 sizeBytes() const;

    void addEditOperation(const EditOperation &operation);
    void clearEditOperations();
    QVector<EditOperation> editOperations() const;
    bool hasPendingEdits() const;
    void setErrorMessage(const QString &message);

private:
    QString m_filePath;
    QString m_errorMessage;
    qint64 m_sizeBytes = 0;
    QVector<EditOperation> m_editOperations;
};
