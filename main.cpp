#include <QApplication>

#include "core/AppController.h"
#include "utils/Logger.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("AtlasHub");
    QApplication::setOrganizationName("AtlasHub");

    Logger::instance().info("Application starting");

    AppController controller;
    controller.initialize();

    const int exitCode = app.exec();
    Logger::instance().info("Application shutting down");
    return exitCode;
}
