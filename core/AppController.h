#pragma once

#include <QRect>

#include <memory>

class MainWindow;
class OCRManager;
class ScreenCapture;
class ScreenOverlay;
class ConfigManager;
class HotkeyManager;

/**
 * @brief Coordinates UI and feature modules.
 *
 * AppController centralizes application flow so presentation code remains isolated
 * from business and infrastructure concerns. This allows modules to evolve
 * independently (e.g., plugin migration or alternate OCR providers).
 */
class AppController
{
public:
    AppController();
    ~AppController();

    AppController(const AppController &) = delete;
    AppController &operator=(const AppController &) = delete;

    void initialize();

private:
    void connectSignals();
    void handleOcrRequested();
    void handleSettingsRequested();
    void handleAreaSelected(const QRect &area);
    void handleSelectionCanceled();
    void openCaptureOverlay();
    void closeCaptureOverlay();

    std::unique_ptr<MainWindow> m_mainWindow;
    std::unique_ptr<OCRManager> m_ocrManager;
    std::unique_ptr<ScreenCapture> m_screenCapture;
    std::unique_ptr<ScreenOverlay> m_screenOverlay;
    std::unique_ptr<ConfigManager> m_configManager;
    std::unique_ptr<HotkeyManager> m_hotkeyManager;
};
