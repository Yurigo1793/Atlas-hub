#include "OCRService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryFile>

namespace {
QString resolveTesseractBinary()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QDir dir(appDir);

    for (int i = 0; i < 6; ++i) {
#ifdef Q_OS_WIN
        const QString candidate = dir.filePath("third_party/tesseract/tesseract.exe");
#else
        const QString candidate = dir.filePath("third_party/tesseract/tesseract");
#endif

        if (QFile::exists(candidate)) {
            return QFileInfo(candidate).absoluteFilePath();
        }

        if (!dir.cdUp()) {
            break;
        }
    }

    return QString();
}
}

QString OCRService::extractText(const QString& imagePath)
{
    const QString tesseractPath = resolveTesseractBinary();

    if (tesseractPath.isEmpty()) {
        return QStringLiteral("Erro: Tesseract não encontrado no pacote do app (third_party/tesseract).");
    }

    const QFileInfo tesseractInfo(tesseractPath);
    qputenv("TESSDATA_PREFIX", tesseractInfo.dir().filePath("tessdata").toUtf8());

    if (!QFile::exists(imagePath)) {
        return QStringLiteral("Falha: imagem temporária para OCR não foi encontrada.");
    }

    QTemporaryFile outputBaseFile(QDir::tempPath() + "/atlas_ocr_output_XXXXXX");
    outputBaseFile.setAutoRemove(false);

    if (!outputBaseFile.open()) {
        return QStringLiteral("Falha ao criar arquivo temporário para saída do OCR.");
    }

    const QString outputBase = outputBaseFile.fileName();
    outputBaseFile.close();
    QFile::remove(outputBase);

    const QString outputTxt = outputBase + ".txt";

    QProcess process;
    process.start(tesseractPath, { imagePath, outputBase, "-l", "por+eng" });

    if (!process.waitForStarted()) {
        return process.errorString();
    }

    if (!process.waitForFinished(-1)) {
        return process.errorString();
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QString stderrOutput = QString::fromUtf8(process.readAllStandardError()).trimmed();
        if (stderrOutput.isEmpty()) {
            stderrOutput = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        }

        if (stderrOutput.isEmpty()) {
            stderrOutput = QStringLiteral("Falha ao executar OCR com o Tesseract.");
        }

        return stderrOutput;
    }

    QFile outputFile(outputTxt);
    if (!outputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QFile::remove(outputTxt);
        return QStringLiteral("Falha ao abrir arquivo de saída do OCR.");
    }

    const QString extractedText = QString::fromUtf8(outputFile.readAll());
    outputFile.close();
    QFile::remove(outputTxt);

    return extractedText;
}
