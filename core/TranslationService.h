#pragma once

#include <QString>

class TranslationService
{
public:
    QString detectLanguage(const QString &text);
    QString translateText(const QString &text, const QString &sourceLanguage, const QString &targetLanguage);
};
