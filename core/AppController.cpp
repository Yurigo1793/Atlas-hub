#include "AppController.h"

#include <QGuiApplication>
#include <QScreen>
#include <QString>
#include <QTimer>

#include "modules/capture/ScreenCapture.h"
#include "modules/capture/ScreenOverlay.h"
#include "modules/ocr/OCRManager.h"
#include "core/HotkeyManager.h"
#include "ui/MainWindow.h"
#include "ui/OCRWindow.h"
#include "utils/ConfigManager.h"
#include "utils/Logger.h"

AppController::AppController()
    : m_mainWindow(std::make_unique<MainWindow>())
    , m_ocrWindow(std::make_unique<OCRWindow>())
    , m_ocrManager(std::make_unique<OCRManager>())
    , m_screenCapture(std::make_unique<ScreenCapture>())
    , m_screenOverlay(std::make_unique<ScreenOverlay>())
    , m_configManager(std::make_unique<ConfigManager>())
    , m_hotkeyManager(std::make_unique<HotkeyManager>())
{
}

AppController::~AppController() = default;

void AppController::initialize()
{
    connectSignals();

    m_configManager->load();
    m_hotkeyManager->initialize();
    m_ocrWindow->setStatusText(QStringLiteral("Ready"));
    m_mainWindow->show();

    Logger::instance().info("Application initialized");
}

void AppController::connectSignals()
{
    QObject::connect(m_mainWindow.get(), &MainWindow::ocrRequested,
                     [this]() { handleOpenOcrRequested(); });

    QObject::connect(m_ocrWindow.get(), &OCRWindow::backRequested,
                     [this]() { handleBackToHubRequested(); });

    QObject::connect(m_ocrWindow.get(), &OCRWindow::captureRequested,
                     [this]() { handleCaptureRequested(); });

    QObject::connect(m_screenOverlay.get(), &ScreenOverlay::areaSelected,
                     [this](const QRect &area) { handleAreaSelected(area); });

    QObject::connect(m_screenOverlay.get(), &ScreenOverlay::selectionCanceled,
                     [this]() { handleSelectionCanceled(); });
}

void AppController::handleOpenOcrRequested()
{
    Logger::instance().info("Opening OCR window");
    m_mainWindow->hide();
    m_ocrWindow->show();
    m_ocrWindow->raise();
    m_ocrWindow->activateWindow();
}

void AppController::handleBackToHubRequested()
{
    Logger::instance().info("Returning to hub");
    m_ocrWindow->hide();
    m_mainWindow->show();
    m_mainWindow->raise();
    m_mainWindow->activateWindow();
}

void AppController::handleCaptureRequested()
{
    Logger::instance().info("OCR capture requested");
    openCaptureOverlay();
}

void AppController::handleAreaSelected(const QRect &area)
{
    const QString coords = QStringLiteral("x=%1 y=%2 w=%3 h=%4")
                               .arg(area.x())
                               .arg(area.y())
                               .arg(area.width())
                               .arg(area.height());

    Logger::instance().info(QStringLiteral("Selected area: x=%1 y=%2 w=%3 h=%4")
                                .arg(area.x())
                                .arg(area.y())
                                .arg(area.width())
                                .arg(area.height()));

    m_ocrWindow->setStatusText(QStringLiteral("Processing... | %1").arg(coords));
    closeCaptureOverlay();

    QTimer::singleShot(80, [this, area]() { processCapturedArea(area); });
}

void AppController::handleSelectionCanceled()
{
    Logger::instance().info("Screen selection canceled");
    closeCaptureOverlay();
    restoreMainWindow();
    m_ocrWindow->setStatusText(QStringLiteral("Ready"));
}

void AppController::openCaptureOverlay()
{
    const ConfigManager::AppSettings settings = m_configManager->appSettings();
    if (!settings.showOverlay) {
        m_ocrWindow->setStatusText(QStringLiteral("Capturing..."));
        m_ocrWindow->hide();
        if (QScreen *screen = QGuiApplication::primaryScreen()) {
            QTimer::singleShot(80, [this, screen]() { processCapturedArea(screen->geometry()); });
        }
        return;
    }

    m_ocrWindow->setStatusText(QStringLiteral("Capturing..."));
    m_ocrWindow->hide();
    m_screenOverlay->showFullScreen();
    m_screenOverlay->raise();
    m_screenOverlay->activateWindow();
    m_screenOverlay->setFocus();
}

void AppController::closeCaptureOverlay()
{
    m_screenOverlay->hide();
}

void AppController::restoreMainWindow()
{
    m_ocrWindow->show();
    m_ocrWindow->raise();
    m_ocrWindow->activateWindow();
}

void AppController::processCapturedArea(const QRect &area)
{
    const QImage capture = m_screenCapture->captureArea(area);
    const bool captureOk = !capture.isNull();
    const QString coords = QStringLiteral("x=%1 y=%2 w=%3 h=%4")
                               .arg(area.x())
                               .arg(area.y())
                               .arg(area.width())
                               .arg(area.height());

    Logger::instance().info(
        QStringLiteral("Capture finished. success=%1 area=[%2]").arg(captureOk ? "true" : "false", coords));

    restoreMainWindow();

    if (!captureOk) {
        m_ocrWindow->setStatusText(QStringLiteral("Capture failed | %1").arg(coords));
        m_ocrWindow->setResultText(QStringLiteral("Falha ao capturar a área selecionada."));
        return;
    }

    const QString extractedText = m_ocrManager->processImage(capture);
    m_ocrWindow->setResultText(extractedText);

    if (extractedText == QStringLiteral("OCR failed")) {
        m_ocrWindow->setStatusText(QStringLiteral("OCR failed | %1").arg(coords));
        return;
    }

    m_ocrWindow->setStatusText(QStringLiteral("Capture complete | %1").arg(coords));
}
