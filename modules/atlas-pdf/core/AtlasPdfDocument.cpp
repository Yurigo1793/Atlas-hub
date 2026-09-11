#include "AtlasPdfDocument.h"

#include <QFileInfo>

AtlasPdfDocument::AtlasPdfDocument(const QString &filePath)
    : m_filePath(filePath)
{
    const QFileInfo info(filePath);
    m_sizeBytes = info.exists() ? info.size() : 0;
}

bool AtlasPdfDocument::isValid() const
{
    return !m_filePath.isEmpty() && m_errorMessage.isEmpty();
}

QString AtlasPdfDocument::filePath() const
{
    return m_filePath;
}

QString AtlasPdfDocument::fileName() const
{
    return QFileInfo(m_filePath).fileName();
}

QString AtlasPdfDocument::errorMessage() const
{
    return m_errorMessage;
}

qint64 AtlasPdfDocument::sizeBytes() const
{
    return m_sizeBytes;
}

void AtlasPdfDocument::addEditOperation(const EditOperation &operation)
{
    m_editOperations.append(operation);
}

void AtlasPdfDocument::clearEditOperations()
{
    m_editOperations.clear();
}

QVector<AtlasPdfDocument::EditOperation> AtlasPdfDocument::editOperations() const
{
    return m_editOperations;
}

bool AtlasPdfDocument::hasPendingEdits() const
{
    return !m_editOperations.isEmpty();
}

void AtlasPdfDocument::setErrorMessage(const QString &message)
{
    m_errorMessage = message;
}
