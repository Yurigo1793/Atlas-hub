#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QIcon>

#include "core/AppController.h"
#include "utils/Logger.h"

namespace
{
QIcon resolveAppIcon()
{
    const QString bundledSvg = QStringLiteral(":/branding/logo.svg");

    const QStringList searchRoots = {
        QCoreApplication::applicationDirPath(),
        QDir::currentPath(),
        QDir::current().filePath(QStringLiteral("assets"))
    };

    for (const QString &root : searchRoots) {
        const QString expectedIco = QDir(root).filePath(QStringLiteral("app.ico"));
        if (QFileInfo::exists(expectedIco)) {
            Logger::instance().info(QStringLiteral("Using application icon: %1").arg(expectedIco));
            return QIcon(expectedIco);
        }

        QDirIterator it(root, {QStringLiteral("*.ico")}, QDir::Files, QDirIterator::NoIteratorFlags);
        while (it.hasNext()) {
            const QString candidate = it.next();
            if (QFileInfo(candidate).fileName().contains(QStringLiteral("ChatGPT"), Qt::CaseInsensitive)) {
                Logger::instance().info(QStringLiteral("Using uploaded .ico icon: %1").arg(candidate));
                return QIcon(candidate);
            }
        }
    }

    Logger::instance().warning(QStringLiteral("No .ico file found. Falling back to bundled SVG icon."));
    return QIcon(bundledSvg);
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("AtlasHub");
    QApplication::setOrganizationName("AtlasHub");
    app.setWindowIcon(resolveAppIcon());

    Logger::instance().info("Application starting");

    AppController controller;
    controller.initialize();

    const int exitCode = app.exec();
    Logger::instance().info("Application shutting down");
    return exitCode;
}
