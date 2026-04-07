#include "MainWindow.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupUiConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUiConnections()
{
    connect(ui->ocrButton, &QPushButton::clicked, this, &MainWindow::ocrRequested);
}
