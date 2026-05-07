#pragma once

#include <QMainWindow>
#include <QString>
#include <QTranslator>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QAction;
class QActionGroup;
class QColor;
class QFontComboBox;
class QKeySequence;
class QMenu;
class QSpinBox;
class QSystemTrayIcon;
class QTextCharFormat;
class QToolBar;
class GlobalHotkey;

class QCloseEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    bool shouldStartMinimizedToTray() const;

public slots:
    void restoreFromTray();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void appendError(const QString &message);
    void applyTextColor(const QColor &color);
    void applyUiLanguage(const QString &localeName);
    int sourceLanguageIndexForTranslateCode(const QString &languageCode) const;
    void loadTranslationLanguageSettings();
    void loadUiLanguageSettings();
    void handleOcrFinished(const QString &path, bool restoreWindowWhenFinished);
    QString ocrHotkeyText() const;
    bool registerOcrHotkey(const QKeySequence &shortcut, bool persist);
    void runTranslation();
    void saveTranslationLanguageSettings() const;
    void saveUiLanguageSettings() const;
    void mergeOutputFormat(const QTextCharFormat &format);
    void refreshTranslationLanguageLabels();
    void retranslateDynamicUi();
    void setupHelp();
    void setupHotkey();
    void setupLanguages();
    void setupTrayIcon();
    void setupTextToolbar();
    void setupUiLanguageMenu();
    void showHotkeySettingsDialog();
    void showHelpWindow();
    void showTrayCloseMessage();
    void startOcrCapture(bool restoreWindowWhenFinished);
    void syncUiLanguageActions();
    void quitFromTray();

    Ui::MainWindow *ui;
    QTranslator m_translator;
    QString m_uiLanguage;
    QString selectedImagePath;

    QToolBar *m_formatToolbar;
    QMenu *m_formatMenu;
    QMenu *m_uiLanguageMenu;
    QActionGroup *m_uiLanguageGroup;
    QAction *m_portugueseAction;
    QAction *m_englishAction;
    QAction *m_frenchAction;
    QAction *m_boldAction;
    QAction *m_italicAction;
    QAction *m_underlineAction;
    QAction *m_colorAction;
    QFontComboBox *m_fontBox;
    QSpinBox *m_sizeBox;
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QAction *m_openAtlasHubAction;
    QAction *m_runOcrAction;
    QAction *m_quitAction;
    QAction *m_configureHotkeyAction;
    GlobalHotkey *m_ocrHotkey;
    QString m_ocrHotkeyText;
    bool m_quitRequested;
    bool m_ocrInProgress;
};
