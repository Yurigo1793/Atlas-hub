#include "OCRService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>

QString OCRService::extractText(const QString& imagePath)
{
    const QString base = QCoreApplication::applicationDirPath();
    const QString tesseractPath = base + "/third_party/tesseract/tesseract.exe";

    if (!QFile::exists(tesseractPath)) {
        return QStringLiteral("Erro: tesseract.exe não encontrado");
    }

    qputenv("TESSDATA_PREFIX", (base + "/third_party/tesseract/tessdata").toUtf8());

    const QString outputBase = QDir::tempPath() + "/atlas_ocr_output";
    const QString outputTxt = outputBase + ".txt";

    QFile::remove(outputTxt);

    QProcess process;
    process.start(tesseractPath, { imagePath, outputBase, "-l", "por+eng" });

    if (!process.waitForStarted()) {
        return process.errorString();
    }

    process.waitForFinished(-1);

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return QString::fromUtf8(process.readAllStandardError());
    }

    QFile outputFile(outputTxt);
    if (!outputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QStringLiteral("Falha ao abrir arquivo de saída do OCR.");
    }

    return QString::fromUtf8(outputFile.readAll());
}
