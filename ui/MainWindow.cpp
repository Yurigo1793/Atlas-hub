#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowIcon(QApplication::windowIcon());
    ui->centralwidget->setStyleSheet(QStringLiteral(
        "QWidget#centralwidget {"
        "background-color: #10141a;"
        "}"));
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
