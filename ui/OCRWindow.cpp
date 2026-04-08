#include "OCRWindow.h"
#include "ui_OCRWindow.h"

#include <QApplication>

OCRWindow::OCRWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::OCRWindow)
{
    ui->setupUi(this);
    setWindowIcon(QApplication::windowIcon());
    ui->centralwidget->setStyleSheet(QStringLiteral(
        "QWidget#centralwidget {"
        "background-color: #10141a;"
        "}"));
    setupUiConnections();
}

OCRWindow::~OCRWindow()
{
    delete ui;
}

void OCRWindow::setStatusText(const QString &text)
{
    ui->textStatus->setPlainText(text);
}

void OCRWindow::setResultText(const QString &text)
{
    ui->textResult->setPlainText(text);
}

void OCRWindow::appendResultText(const QString &text)
{
    ui->textResult->append(text);
}

void OCRWindow::setupUiConnections()
{
    connect(ui->captureButton, &QPushButton::clicked, this, &OCRWindow::captureRequested);
    connect(ui->backButton, &QPushButton::clicked, this, &OCRWindow::backRequested);
}
