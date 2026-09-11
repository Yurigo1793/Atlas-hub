#include "OCRService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>

namespace {

struct TesseractRuntime {
    QString executablePath;
    QString tessdataPath;
    QString errorMessage;
};

TesseractRuntime tesseractRuntime(const QString &language)
{
    const QString base = QCoreApplication::applicationDirPath();

    TesseractRuntime runtime;
    runtime.executablePath = base + "/modules/atlas-ocr/third_party/tesseract/tesseract.exe";
    runtime.tessdataPath = base + "/modules/atlas-ocr/third_party/tesseract/tessdata";

    if (!QFile::exists(runtime.executablePath)) {
        runtime.errorMessage = QCoreApplication::translate("OCRService",
                                                           "Erro: Tesseract empacotado não encontrado em: %1")
                                   .arg(runtime.executablePath);
        return runtime;
    }

    if (!QDir(runtime.tessdataPath).exists()) {
        runtime.errorMessage = QCoreApplication::translate("OCRService", "Erro: tessdata não encontrado em: %1")
                                   .arg(runtime.tessdataPath);
        return runtime;
    }

    const QStringList languages = language.split(QLatin1Char('+'), Qt::SkipEmptyParts);
    for (const QString &singleLanguage : languages) {
        const QString tessdataFile = runtime.tessdataPath + "/" + singleLanguage + ".traineddata";
        if (!QFile::exists(tessdataFile)) {
            runtime.errorMessage = QCoreApplication::translate(
                                       "OCRService",
                                       "Erro: dados de OCR não encontrados para o idioma selecionado: %1")
                                       .arg(tessdataFile);
            return runtime;
        }
    }

    return runtime;
}

QString runTesseract(const QString &imagePath,
                     const QString &language,
                     const QStringList &extraArguments,
                     const QString &extension)
{
    const TesseractRuntime runtime = tesseractRuntime(language);
    if (!runtime.errorMessage.isEmpty()) {
        return runtime.errorMessage;
    }

    if (!QFile::exists(imagePath)) {
        return QCoreApplication::translate("OCRService",
                                           "Falha: imagem temporária para OCR não foi encontrada.");
    }

    QProcess process;
    process.setProgram(runtime.executablePath);
    process.setWorkingDirectory(QFileInfo(runtime.executablePath).absolutePath());
    process.setProcessChannelMode(QProcess::MergedChannels);

    const QString outputBase =
        QDir::tempPath() + "/atlas_ocr_" + QString::number(QDateTime::currentMSecsSinceEpoch());

    QStringList args;
    args << imagePath
         << outputBase
         << "-l" << language
         << "--tessdata-dir" << runtime.tessdataPath;
    args << extraArguments;

    process.setArguments(args);
    process.start();

    if (!process.waitForStarted()) {
        return QCoreApplication::translate("OCRService", "Erro ao iniciar OCR: %1")
            .arg(process.errorString());
    }

    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished();
        return QCoreApplication::translate("OCRService", "Erro ao executar OCR: %1")
            .arg(process.errorString());
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return QCoreApplication::translate("OCRService", "Erro no OCR: %1")
            .arg(QString::fromUtf8(process.readAll()));
    }

    QFile outputFile(outputBase + extension);
    if (!outputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QCoreApplication::translate("OCRService", "Falha ao abrir arquivo de saída do OCR.");
    }

    const QString extractedText = QString::fromUtf8(outputFile.readAll());
    outputFile.close();
    QFile::remove(outputBase + extension);

    return extractedText;
}

} // namespace

QString OCRService::extractText(const QString &imagePath, const QString &language)
{
    return runTesseract(imagePath, language, QStringList(), QStringLiteral(".txt"));
}

QVector<OCRService::OcrWord> OCRService::extractWords(const QString &imagePath, const QString &language)
{
    QVector<OcrWord> words;
    const QString tsv = runTesseract(imagePath,
                                     language,
                                     QStringList{QStringLiteral("-c"), QStringLiteral("tessedit_create_tsv=1")},
                                     QStringLiteral(".tsv"));
    if (tsv.startsWith(QStringLiteral("Erro:"), Qt::CaseInsensitive)
        || tsv.startsWith(QStringLiteral("Falha:"), Qt::CaseInsensitive)) {
        return words;
    }

    const QStringList lines = tsv.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (int i = 1; i < lines.size(); ++i) {
        const QStringList columns = lines.at(i).split(QLatin1Char('\t'));
        if (columns.size() < 12 || columns.at(0).toInt() != 5) {
            continue;
        }

        const QString text = columns.mid(11).join(QLatin1Char('\t')).trimmed();
        if (text.isEmpty()) {
            continue;
        }

        OcrWord word;
        word.text = text;
        word.blockNumber = columns.at(2).toInt();
        word.paragraphNumber = columns.at(3).toInt();
        word.lineNumber = columns.at(4).toInt();
        word.wordNumber = columns.at(5).toInt();
        word.bounds = QRect(columns.at(6).toInt(),
                            columns.at(7).toInt(),
                            columns.at(8).toInt(),
                            columns.at(9).toInt());
        word.confidence = columns.at(10).toDouble();
        words.push_back(word);
    }

    return words;
}

QVector<OCRService::OcrCharacter> OCRService::extractCharacters(const QString &imagePath, const QString &language)
{
    QVector<OcrCharacter> characters;
    const QString hocr = runTesseract(imagePath,
                                      language,
                                      QStringList{QStringLiteral("-c"),
                                                  QStringLiteral("tessedit_create_hocr=1"),
                                                  QStringLiteral("-c"),
                                                  QStringLiteral("hocr_char_boxes=1")},
                                      QStringLiteral(".hocr"));
    if (!hocr.startsWith(QStringLiteral("Erro:"), Qt::CaseInsensitive)
        && !hocr.startsWith(QStringLiteral("Falha:"), Qt::CaseInsensitive)) {
        const QRegularExpression baselineExpression(QStringLiteral(R"(baseline\s+([+-]?\d+(?:\.\d+)?)\s+([+-]?\d+(?:\.\d+)?))"));
        const QRegularExpression characterExpression(
            QStringLiteral(R"(<span class='ocrx_cinfo' title='x_bboxes\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+);\s+x_conf\s+([+-]?\d+(?:\.\d+)?)'>(.*?)</span>)"));

        int lineNumber = 0;
        double currentAngle = 0.0;
        const QStringList hocrLines = hocr.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &hocrLine : hocrLines) {
            if (hocrLine.contains(QStringLiteral("class='ocr_line'"))) {
                ++lineNumber;
                currentAngle = 0.0;
                const QRegularExpressionMatch baselineMatch = baselineExpression.match(hocrLine);
                if (baselineMatch.hasMatch()) {
                    const double baselineSlope = baselineMatch.captured(1).toDouble();
                    currentAngle = std::atan(baselineSlope) * 180.0 / 3.14159265358979323846;
                }
            }

            const QRegularExpressionMatch characterMatch = characterExpression.match(hocrLine);
            if (!characterMatch.hasMatch()) {
                continue;
            }

            const int left = characterMatch.captured(1).toInt();
            const int top = characterMatch.captured(2).toInt();
            const int right = characterMatch.captured(3).toInt();
            const int bottom = characterMatch.captured(4).toInt();
            if (right <= left || bottom <= top) {
                continue;
            }

            OcrCharacter character;
            character.text = characterMatch.captured(6);
            character.text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
            character.text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
            character.text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
            character.text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
            character.text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
            character.bounds = QRect(left, top, right - left, bottom - top);
            character.lineNumber = lineNumber;
            character.confidence = characterMatch.captured(5).toDouble();
            character.angleDegrees = currentAngle;
            character.topLeftOrigin = true;
            characters.push_back(character);
        }

        if (!characters.isEmpty()) {
            return characters;
        }
    }

    const QString boxText = runTesseract(imagePath,
                                         language,
                                         QStringList{QStringLiteral("-c"), QStringLiteral("tessedit_create_boxfile=1")},
                                         QStringLiteral(".box"));
    if (boxText.startsWith(QStringLiteral("Erro:"), Qt::CaseInsensitive)
        || boxText.startsWith(QStringLiteral("Falha:"), Qt::CaseInsensitive)) {
        return characters;
    }

    const QStringList lines = boxText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList columns = line.simplified().split(QLatin1Char(' '));
        if (columns.size() < 5) {
            continue;
        }

        bool leftOk = false;
        bool bottomOk = false;
        bool rightOk = false;
        bool topOk = false;
        const int left = columns.at(1).toInt(&leftOk);
        const int bottom = columns.at(2).toInt(&bottomOk);
        const int right = columns.at(3).toInt(&rightOk);
        const int top = columns.at(4).toInt(&topOk);
        if (!leftOk || !bottomOk || !rightOk || !topOk || right <= left || top <= bottom) {
            continue;
        }

        OcrCharacter character;
        character.text = columns.at(0);
        character.bounds = QRect(left, bottom, right - left, top - bottom);
        character.lineNumber = characters.size();
        characters.push_back(character);
    }

    return characters;
}
