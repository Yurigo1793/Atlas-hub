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

void MainWindow::setResultText(const QString &text)
{
    ui->ocrResultTextEdit->setPlainText(text);
}

void MainWindow::appendResultText(const QString &text)
{
    ui->ocrResultTextEdit->append(text);
}

void MainWindow::setStatusText(const QString &text)
{
    ui->statusLabel->setText(text);
}

void MainWindow::setupUiConnections()
{
    connect(ui->ocrButton, &QPushButton::clicked, this, &MainWindow::ocrRequested);
    connect(ui->settingsButton, &QPushButton::clicked, this, &MainWindow::settingsRequested);
}
