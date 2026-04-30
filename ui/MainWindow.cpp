#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "core/OCRService.h"
#include "ui/ScreenCaptureOverlay.h"

#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->btnRunOCR, &QPushButton::clicked, this, [this]() {
        this->showMinimized();

        QTimer::singleShot(300, this, [this]() {
            auto *overlay = new ScreenCaptureOverlay(this);

            connect(overlay, &ScreenCaptureOverlay::captureFinished, this, [this](const QString &path) {
                this->showNormal();
                this->raise();
                this->activateWindow();

                if (path.isEmpty()) {
                    ui->textOutput->setPlainText("Falha ao capturar a área selecionada.");
                    return;
                }

                OCRService ocrService;
                const QString result = ocrService.extractText(path);
                ui->textOutput->setPlainText(result);
            });

            overlay->showFullScreen();
        });
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}
