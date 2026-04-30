#include "OCRService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>

QString OCRService::extractText(const QString& imagePath)
{
    const QString tesseractRoot = QCoreApplication::applicationDirPath() + "/third_party/tesseract";
    const QString tesseractExe = QDir::toNativeSeparators(tesseractRoot + "/tesseract.exe");

    const QString outputBase = QDir::tempPath() + "/atlas_ocr_output";
    const QString outputTxt = outputBase + ".txt";

    QFile::remove(outputTxt);

    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("TESSDATA_PREFIX", QDir::toNativeSeparators(tesseractRoot + "/tessdata"));
    process.setProcessEnvironment(env);

    process.start(tesseractExe, { imagePath, outputBase, "-l", "por+eng" });

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
