#include "OCRService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

QString OCRService::extractText(const QString &imagePath, const QString &language)
{
    const QString base = QCoreApplication::applicationDirPath();

    const QString tesseractPath =
        base + "/third_party/tesseract/tesseract.exe";

    const QString tessdataPath =
        base + "/third_party/tesseract/tessdata";

    if (!QFile::exists(tesseractPath)) {
        return QCoreApplication::translate("OCRService",
                                           "Erro: Tesseract empacotado não encontrado em: %1")
            .arg(tesseractPath);
    }

    if (!QDir(tessdataPath).exists()) {
        return QCoreApplication::translate("OCRService", "Erro: tessdata não encontrado em: %1")
            .arg(tessdataPath);
    }

    const QString tessdataFile = tessdataPath + "/" + language + ".traineddata";
    if (!QFile::exists(tessdataFile)) {
        return QCoreApplication::translate("OCRService",
                                           "Erro: dados de OCR não encontrados para o idioma selecionado: %1")
            .arg(tessdataFile);
    }

    if (!QFile::exists(imagePath)) {
        return QCoreApplication::translate("OCRService",
                                           "Falha: imagem temporária para OCR não foi encontrada.");
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
         << "-l" << language
         << "--tessdata-dir" << tessdataPath;

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

    QFile outputFile(outputBase + ".txt");
    if (!outputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QCoreApplication::translate("OCRService", "Falha ao abrir arquivo de saída do OCR.");
    }

    const QString extractedText = QString::fromUtf8(outputFile.readAll());
    outputFile.close();
    QFile::remove(outputBase + ".txt");

    return extractedText;
}
