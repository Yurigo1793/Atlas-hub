#pragma once

#include <QRect>
#include <QString>
#include <QVector>

class OCRService
{
public:
    struct OcrWord {
        QString text;
        QRect bounds;
        int blockNumber = 0;
        int paragraphNumber = 0;
        int lineNumber = 0;
        int wordNumber = 0;
        double confidence = -1.0;
        double angleDegrees = 0.0;
    };

    struct OcrCharacter {
        QString text;
        QRect bounds;
        int lineNumber = 0;
        double confidence = -1.0;
        double angleDegrees = 0.0;
        bool topLeftOrigin = false;
    };

    QString extractText(const QString &imagePath, const QString &language);
    QVector<OcrWord> extractWords(const QString &imagePath, const QString &language);
    QVector<OcrCharacter> extractCharacters(const QString &imagePath, const QString &language);
};
