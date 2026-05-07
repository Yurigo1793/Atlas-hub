#pragma once

#include <QString>

class OCRService
{
public:
    QString extractText(const QString &imagePath, const QString &language);
};
