#include <QApplication>
#include <QCoreApplication>
#include <QIcon>

#include "core/AppController.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("AtlasHub");
    QCoreApplication::setApplicationName("AtlasHub");
    QApplication::setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(":/icons/app.ico"));

    AppController controller;
    controller.initialize();

    return app.exec();
}
