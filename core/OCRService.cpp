#include "OCRService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>

QString OCRService::extractText(const QString& imagePath)
{
    QString base = QCoreApplication::applicationDirPath();

    QString tesseractPath =
        base + "/third_party/tesseract/tesseract.exe";

    QString tessdataPath =
        base + "/third_party/tesseract/tessdata";

    if (!QFile::exists(tesseractPath)) {
        return "Erro: tesseract.exe não encontrado em: " + tesseractPath;
    }

    qputenv("TESSDATA_PREFIX", tessdataPath.toUtf8());

    if (!QFile::exists(imagePath)) {
        return QStringLiteral("Falha: imagem temporária para OCR não foi encontrada.");
    }

    QProcess process;
    process.setProgram(tesseractPath);

    QString outputBase = QDir::tempPath() + "/atlas_ocr";

    QStringList args;
    args << imagePath << outputBase << "-l" << "por+eng";

    process.setArguments(args);
    process.start();

    if (!process.waitForFinished()) {
        return "Erro ao executar OCR: " + process.errorString();
    }

    QFile outputFile(outputBase + ".txt");
    if (!outputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QStringLiteral("Falha ao abrir arquivo de saída do OCR.");
    }

    const QString extractedText = QString::fromUtf8(outputFile.readAll());
    outputFile.close();
    QFile::remove(outputBase + ".txt");

    return extractedText;
}
