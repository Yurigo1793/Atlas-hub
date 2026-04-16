#include "AppController.h"

#include "ui/MainWindow.h"

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_mainWindow(nullptr)
{
}

AppController::~AppController()
{
    delete m_mainWindow;
}

void AppController::initialize()
{
    if (!m_mainWindow) {
        m_mainWindow = new MainWindow();
    }

    m_mainWindow->show();
}
