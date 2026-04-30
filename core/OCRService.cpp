#include "OCRService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

QString OCRService::extractText(const QString& imagePath)
{
    const QString base = QCoreApplication::applicationDirPath();

    const QString tesseractPath =
        base + "/third_party/tesseract/tesseract.exe";

    const QString tessdataPath =
        base + "/third_party/tesseract/tessdata";

    if (!QFile::exists(tesseractPath)) {
        return "Erro: Tesseract empacotado nao encontrado em: " + tesseractPath;
    }

    if (!QDir(tessdataPath).exists()) {
        return "Erro: tessdata nao encontrado em: " + tessdataPath;
    }

    if (!QFile::exists(imagePath)) {
        return QStringLiteral("Falha: imagem temporaria para OCR nao foi encontrada.");
    }

    QProcess process;
    process.setProgram(tesseractPath);
    process.setWorkingDirectory(QFileInfo(tesseractPath).absolutePath());
    process.setProcessChannelMode(QProcess::MergedChannels);

    const QString outputBase =
        QDir::tempPath() + "/atlas_ocr_" + QString::number(QDateTime::currentMSecsSinceEpoch());

    QStringList args;
    args << imagePath
         << outputBase
         << "-l" << "por+eng"
         << "--tessdata-dir" << tessdataPath;

    process.setArguments(args);
    process.start();

    if (!process.waitForStarted()) {
        return "Erro ao iniciar OCR: " + process.errorString();
    }

    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished();
        return "Erro ao executar OCR: " + process.errorString();
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return QStringLiteral("Erro no OCR: ") + QString::fromUtf8(process.readAll());
    }

    QFile outputFile(outputBase + ".txt");
    if (!outputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QStringLiteral("Falha ao abrir arquivo de saida do OCR.");
    }

    const QString extractedText = QString::fromUtf8(outputFile.readAll());
    outputFile.close();
    QFile::remove(outputBase + ".txt");

    return extractedText;
}
