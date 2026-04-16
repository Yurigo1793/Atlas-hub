#include <QApplication>

#include "core/AppController.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    AppController controller;
    controller.initialize();

    return app.exec();
}
