#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "core/OCRService.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->btnSelectImage, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(
            this,
            "Selecionar imagem",
            "",
            "Images (*.png *.jpg *.jpeg *.bmp)"
        );

        if (!file.isEmpty()) {
            selectedImagePath = file;
            ui->lblImagePath->setText(file);
        }
    });

    connect(ui->btnRunOCR, &QPushButton::clicked, this, [this]() {
        if (selectedImagePath.isEmpty()) {
            QMessageBox::warning(this, "Aviso", "Selecione uma imagem antes de executar o OCR.");
            return;
        }

        OCRService ocrService;
        QString result = ocrService.extractText(selectedImagePath);
        ui->textOutput->setPlainText(result);
    });

    connect(ui->btnCopy, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(ui->textOutput->toPlainText());
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}
