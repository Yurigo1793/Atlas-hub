#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "core/OCRService.h"
#include "ui/ScreenCaptureOverlay.h"

#include <QCoreApplication>
#include <QFile>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->btnRunOCR, &QPushButton::clicked, this, [this]() {
        ui->textOutput->setPlainText("Selecione a area da tela para capturar...");

        this->showMinimized();

        QTimer::singleShot(300, this, [this]() {
            auto *overlay = new ScreenCaptureOverlay();

            connect(overlay, &ScreenCaptureOverlay::captureFinished, this, [this](const QString &path) {
                this->showNormal();
                this->raise();
                this->activateWindow();

                if (path.isEmpty() || !QFile::exists(path)) {
                    ui->textOutput->setPlainText("Falha ao capturar a area selecionada.");
                    return;
                }

                ui->textOutput->setPlainText("Imagem capturada. Executando OCR...");
                QCoreApplication::processEvents();

                OCRService ocrService;
                const QString result = ocrService.extractText(path);
                const QString visibleResult = result.trimmed();

                if (visibleResult.isEmpty()) {
                    ui->textOutput->setPlainText("OCR concluido, mas nenhum texto foi reconhecido.");
                    return;
                }

                ui->textOutput->setPlainText(result);
                ui->textOutput->setFocus();
            });

            overlay->showFullScreen();
        });
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}
