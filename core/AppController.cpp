#include "AppController.h"

#include <QString>

#include "modules/capture/ScreenCapture.h"
#include "modules/capture/ScreenOverlay.h"
#include "modules/ocr/OCRManager.h"
#include "core/HotkeyManager.h"
#include "ui/MainWindow.h"
#include "utils/ConfigManager.h"
#include "utils/Logger.h"

AppController::AppController()
    : m_mainWindow(std::make_unique<MainWindow>())
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
    m_mainWindow->setStatusText(QStringLiteral("Ready"));
    m_mainWindow->show();

    Logger::instance().info("Application initialized");
}

void AppController::connectSignals()
{
    QObject::connect(m_mainWindow.get(), &MainWindow::ocrRequested,
                     [this]() { handleOcrRequested(); });

    QObject::connect(m_mainWindow.get(), &MainWindow::settingsRequested,
                     [this]() { handleSettingsRequested(); });

    QObject::connect(m_screenOverlay.get(), &ScreenOverlay::areaSelected,
                     [this](const QRect &area) { handleAreaSelected(area); });

    QObject::connect(m_screenOverlay.get(), &ScreenOverlay::selectionCanceled,
                     [this]() { handleSelectionCanceled(); });
}

void AppController::handleOcrRequested()
{
    Logger::instance().info("OCR flow requested");
    openCaptureOverlay();
}

void AppController::handleSettingsRequested()
{
    Logger::instance().info("Settings action triggered");
    m_mainWindow->appendResultText("[INFO] Configurações ainda não implementadas.");
}

void AppController::handleAreaSelected(const QRect &area)
{
    Logger::instance().info(QStringLiteral("Selected area: x=%1 y=%2 w=%3 h=%4")
                                .arg(area.x())
                                .arg(area.y())
                                .arg(area.width())
                                .arg(area.height()));

    closeCaptureOverlay();

    const QImage capture = m_screenCapture->captureFullScreen();
    const QString extractedText = m_ocrManager->extractText(capture);
    m_mainWindow->setResultText(QStringLiteral("%1\nÁrea selecionada: [%2, %3, %4, %5]")
                                    .arg(extractedText)
                                    .arg(area.x())
                                    .arg(area.y())
                                    .arg(area.width())
                                    .arg(area.height()));
    m_mainWindow->setStatusText(QStringLiteral("Ready"));
}

void AppController::handleSelectionCanceled()
{
    Logger::instance().info("Screen selection canceled");
    closeCaptureOverlay();
    m_mainWindow->setStatusText(QStringLiteral("Ready"));
}

void AppController::openCaptureOverlay()
{
    const ConfigManager::AppSettings settings = m_configManager->appSettings();
    if (!settings.showOverlay) {
        m_mainWindow->setStatusText(QStringLiteral("Capturing..."));
        handleAreaSelected(QRect());
        return;
    }

    m_mainWindow->setStatusText(QStringLiteral("Capturing..."));
    m_mainWindow->hide();
    m_screenOverlay->show();
    m_screenOverlay->raise();
    m_screenOverlay->activateWindow();
    m_screenOverlay->setFocus();
}

void AppController::closeCaptureOverlay()
{
    m_screenOverlay->hide();
    m_mainWindow->show();
    m_mainWindow->raise();
    m_mainWindow->activateWindow();
}
