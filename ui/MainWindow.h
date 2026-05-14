#pragma once

#include <QMainWindow>
#include <QList>
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
class QDockWidget;
class QFontComboBox;
class QKeySequence;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QSpinBox;
class QSystemTrayIcon;
class QTextCharFormat;
class QToolButton;
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
    void applyTheme();
    void applyUiLanguage(const QString &localeName);
    void addHistoryEntry(const QString &ocrText, const QString &translationText);
    void copyTextToClipboard(const QString &text, const QString &emptyMessage, const QString &successMessage);
    QString currentApplicationPath() const;
    bool isWindowsStartupEnabled() const;
    int sourceLanguageIndexForTranslateCode(const QString &languageCode) const;
    void loadHistory();
    void loadTranslationLanguageSettings();
    void loadUiLanguageSettings();
    void handleOcrFinished(const QString &path, bool restoreWindowWhenFinished);
    void refreshHistoryView();
    QString ocrHotkeyText() const;
    bool registerOcrHotkey(const QKeySequence &shortcut, bool persist);
    void runTranslation();
    void saveHistory() const;
    void saveTranslationLanguageSettings() const;
    void saveUiLanguageSettings() const;
    void setWindowsStartupEnabled(bool enabled);
    void mergeOutputFormat(const QTextCharFormat &format);
    void refreshTranslationLanguageLabels();
    void retranslateDynamicUi();
    void setupCopyButtons();
    void setupHelp();
    void setupHistory();
    void setupHotkey();
    void setupLanguages();
    void setupSettingsActions();
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
    QAction *m_startWithWindowsAction;
    QAction *m_startMinimizedAction;
    QAction *m_darkThemeAction;
    QAction *m_clearHistoryAction;
    QToolButton *m_copyOcrButton;
    QToolButton *m_copyTranslationButton;
    QDockWidget *m_historyDock;
    QListWidget *m_historyList;
    GlobalHotkey *m_ocrHotkey;
    QString m_ocrHotkeyText;
    struct HistoryEntry
    {
        QString timestamp;
        QString ocrText;
        QString translationText;
    };
    QList<HistoryEntry> m_history;
    bool m_quitRequested;
    bool m_ocrInProgress;
    bool m_darkTheme;
};
