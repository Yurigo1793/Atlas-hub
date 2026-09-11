#include "AtlasPdfEditorWindow.h"
#include "AtlasPdfTextEngine.h"

#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QIcon>
#include <QPdfDocument>
#include <QRectF>
#include <QStringList>
#include <QTextStream>
#include <QThread>
#include <QTimer>

int main(int argc, char *argv[])
{
    QStringList arguments;
    arguments.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        arguments.push_back(QString::fromLocal8Bit(argv[i]));
    }
    if (arguments.size() == 8 && arguments.at(1) == QStringLiteral("--selection-probe")) {
        QTextStream output(stdout);
        QTextStream error(stderr);

        bool ok = false;
        const int pageIndex = arguments.at(3).toInt(&ok) - 1;
        if (!ok || pageIndex < 0) {
            error << "Invalid page." << Qt::endl;
            return 2;
        }

        bool x1Ok = false;
        bool y1Ok = false;
        bool x2Ok = false;
        bool y2Ok = false;
        const qreal x1 = arguments.at(4).toDouble(&x1Ok);
        const qreal y1 = arguments.at(5).toDouble(&y1Ok);
        const qreal x2 = arguments.at(6).toDouble(&x2Ok);
        const qreal y2 = arguments.at(7).toDouble(&y2Ok);
        if (!x1Ok || !y1Ok || !x2Ok || !y2Ok) {
            error << "Invalid selection rectangle." << Qt::endl;
            return 2;
        }

        const QRectF rect(QPointF(x1, y1), QPointF(x2, y2));
        QString probeReport = AtlasPdfTextEngine::visualTextInRect(arguments.at(2), pageIndex, QSizeF(), rect) + QLatin1Char('\n');
        if (qEnvironmentVariableIsSet("ATLAS_PDF_PROBE_DEBUG")) {
            probeReport += QStringLiteral("probe=structured-fallback\n");
        }
        output << probeReport << Qt::endl;
        const QString outputPath = qEnvironmentVariable("ATLAS_PDF_PROBE_OUTPUT");
        if (!outputPath.isEmpty()) {
            QFile reportFile(outputPath);
            if (reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                reportFile.write(probeReport.toUtf8());
            }
        }
        return 0;
    }

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("AtlasHub");
    QCoreApplication::setApplicationName("AtlasPdfEditor");
    QCoreApplication::setApplicationVersion("0.1.0");
    app.setWindowIcon(QIcon(":/icons/app.ico"));

    AtlasPdfEditorWindow window;
    window.show();
    if (arguments.size() > 1) {
        QString filePath = arguments.at(1);
        if (!QFile::exists(filePath)) {
            const QString joinedFilePath = arguments.mid(1).join(QLatin1Char(' '));
            if (QFile::exists(joinedFilePath)) {
                filePath = joinedFilePath;
            }
        }
        QTimer::singleShot(0, &window, [filePath, &window]() {
            window.openPdfFile(filePath);
        });
    }

    return app.exec();
}
