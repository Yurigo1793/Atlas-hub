#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "core/TranslationService.h"
#include "modules/atlas-ocr/ui/OCRService.h"
#include "modules/atlas-ocr/ui/ScreenCaptureOverlay.h"
#include "utils/GlobalHotkey.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDir>
#include <QFile>
#include <QFontComboBox>
#include <QFormLayout>
#include <QIcon>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSize>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QToolBar>
#include <QVBoxLayout>

namespace {
struct LanguageOption
{
    const char *label;
    const char *ocrCode;
    const char *translateCode;
};

const LanguageOption languageOptions[] = {
    {QT_TRANSLATE_NOOP("MainWindow", "Português"), "por", "pt"},
    {QT_TRANSLATE_NOOP("MainWindow", "Inglês"), "eng", "en"},
    {QT_TRANSLATE_NOOP("MainWindow", "Espanhol"), "spa", "es"},
    {QT_TRANSLATE_NOOP("MainWindow", "Francês"), "fra", "fr"},
};

bool isErrorMessage(const QString &message)
{
    return message.startsWith(QCoreApplication::translate("MainWindow", "Erro"), Qt::CaseInsensitive)
           || message.startsWith(QCoreApplication::translate("MainWindow", "Falha"), Qt::CaseInsensitive)
           || message.startsWith(QStringLiteral("Erro"), Qt::CaseInsensitive)
           || message.startsWith(QStringLiteral("Falha"), Qt::CaseInsensitive);
}

QKeySequence defaultOcrHotkey()
{
    return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Backslash);
}

struct TranslationResult
{
    QString sourceText;
    QString detectedLanguage;
    QString sourceLanguage;
    QString targetLanguage;
    QString translatedText;
};

QString darkThemeStyleSheet()
{
    return QStringLiteral(
        "* { font-family: System; font-size: 12pt; }"
        "QMainWindow { background-color: #101214; color: #d7dde3; }"
        "QWidget#centralwidget { background-color: #101214; }"
        "QMenuBar { background: #15191d; color: #d7dde3; border: 1px solid #2b333a; font-weight: bold; }"
        "QMenuBar::item { background: transparent; padding: 2px 10px; }"
        "QMenuBar::item:selected { background: #1f7a4d; color: #f5fff8; }"
        "QMenu { background: #15191d; color: #d7dde3; border: 1px solid #2f3a42; font-weight: bold; }"
        "QMenu::item { padding: 4px 26px 4px 16px; }"
        "QMenu::item:selected { background: #1f7a4d; color: #f5fff8; }"
        "QMenu::separator { height: 1px; background: #33404a; margin: 4px 8px; }"
        "QLabel, QLineEdit, QComboBox, QTextEdit, QPlainTextEdit, QListWidget {"
        " background: #171b1f; color: #d7dde3; border: 1px solid #34404a; font-weight: bold;"
        " selection-background-color: #238456; selection-color: #f5fff8; }"
        "QTextEdit, QPlainTextEdit, QListWidget { padding: 4px; background: #12161a; }"
        "QListWidget { alternate-background-color: #1a1f24; }"
        "QListWidget::item { background: #12161a; color: #d7dde3; }"
        "QListWidget::item:alternate { background: #1a1f24; color: #c7d0d8; }"
        "QListWidget::item:selected { background: #238456; color: #f5fff8; }"
        "QTextEdit:focus, QPlainTextEdit:focus, QLineEdit:focus, QComboBox:focus, QListWidget:focus {"
        " border: 1px solid #2aa866; }"
        "QScrollBar:vertical, QScrollBar:horizontal { background: #111519; border: 1px solid #303a43; width: 14px; height: 14px; }"
        "QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: #4b5a64; min-height: 20px; min-width: 20px; }"
        "QScrollBar::handle:hover { background: #238456; }"
        "QScrollBar::add-line, QScrollBar::sub-line { background: #171b1f; border: 1px solid #303a43; }"
        "QScrollBar::add-page, QScrollBar::sub-page { background: #111519; }"
        "QPushButton, QToolButton {"
        " background: #1c2227; color: #e3e8ed; border-top: 2px solid #51616d;"
        " border-left: 2px solid #51616d; border-right: 4px solid #07090b;"
        " border-bottom: 4px solid #07090b; padding: 4px 8px; font-weight: bold; }"
        "QPushButton:hover, QToolButton:hover { background: #243039; color: #ffffff; }"
        "QPushButton:pressed, QToolButton:pressed {"
        " background: #238456; color: #f5fff8; border-top: 4px solid #07090b;"
        " border-left: 4px solid #07090b; border-right: 2px solid #51616d;"
        " border-bottom: 2px solid #51616d; padding-top: 6px; padding-left: 10px;"
        " padding-right: 6px; padding-bottom: 2px; }"
        "QPushButton#btnRunOCR, QPushButton#btnTranslate { background-color: #238456; color: #f5fff8; }"
        "QComboBox::drop-down { border-left: 1px solid #34404a; background: #1f262c; }"
        "QCheckBox { color: #d7dde3; background: transparent; border: none; font-weight: bold; }"
        "QDockWidget { background-color: #101214; color: #d7dde3; font-weight: bold; }"
        "QDockWidget::title { background: #15191d; border: 1px solid #2f3a42; padding: 2px; }"
        "QStatusBar { background: #15191d; color: #9fb0bc; border-top: 1px solid #2f3a42; font-weight: bold; }"
        "QStatusBar QLabel { background: transparent; border: none; color: #9fb0bc; padding: 0 10px; }"
        "QLabel#systemStatusLabel { color: #3fd07f; }"
        "QLabel#engineStatusLabel { color: #7f8f9c; }"
        "QLabel#ocrStatusLabel, QLabel#translationStatusLabel { color: #c7d0d8; }");
}

QString trayMenuStyleSheet(bool darkTheme)
{
    if (darkTheme) {
        return QStringLiteral(
            "QMenu { background: #15191d; color: #d7dde3; border: 1px solid #2f3a42; min-width: 210px; font-family: System; font-size: 12pt; }"
            "QMenu::item { padding: 5px 24px 5px 16px; }"
            "QMenu::item:selected { background: #238456; color: #f5fff8; }"
            "QMenu::separator { height: 1px; background: #33404a; margin: 4px 8px; }");
    }

    return QStringLiteral(
        "QMenu { background: #f4f1e8; color: #111111; border: 1px solid #111111; min-width: 210px; font-family: System; font-size: 12pt; }"
        "QMenu::item { padding: 5px 24px 5px 16px; }"
        "QMenu::item:selected { background: #00FF66; color: #111111; }"
        "QMenu::separator { height: 1px; background: #aaa69b; margin: 4px 8px; }");
}

QPixmap atlasHubWindowIconPixmap(bool darkTheme, int size)
{
    const QString path = darkTheme
                             ? QStringLiteral(":/icons/atlas_globe_dark.svg")
                             : QStringLiteral(":/icons/atlas_globe_light.svg");
    return QIcon(path).pixmap(size, size);
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_uiLanguage(QStringLiteral("pt_BR"))
    , m_lightStyleSheet()
    , m_formatToolbar(nullptr)
    , m_formatMenu(nullptr)
    , m_uiLanguageMenu(nullptr)
    , m_uiLanguageGroup(nullptr)
    , m_portugueseAction(nullptr)
    , m_englishAction(nullptr)
    , m_frenchAction(nullptr)
    , m_boldAction(nullptr)
    , m_italicAction(nullptr)
    , m_underlineAction(nullptr)
    , m_colorAction(nullptr)
    , m_fontBox(nullptr)
    , m_sizeBox(nullptr)
    , m_trayIcon(nullptr)
    , m_trayMenu(nullptr)
    , m_openAtlasHubAction(nullptr)
    , m_runOcrAction(nullptr)
    , m_quitAction(nullptr)
    , m_configureHotkeyAction(nullptr)
    , m_startWithWindowsAction(nullptr)
    , m_startMinimizedAction(nullptr)
    , m_darkThemeAction(nullptr)
    , m_clearHistoryAction(nullptr)
    , m_smartLanguagesAction(nullptr)
    , m_copyOcrButton(nullptr)
    , m_copyTranslationButton(nullptr)
    , m_historyDock(nullptr)
    , m_historySearchEdit(nullptr)
    , m_historyDateFilterCombo(nullptr)
    , m_historyFavoritesOnlyCheck(nullptr)
    , m_historyFavoriteButton(nullptr)
    , m_historyList(nullptr)
    , m_systemStatusLabel(nullptr)
    , m_engineStatusLabel(nullptr)
    , m_ocrStatusLabel(nullptr)
    , m_translationStatusLabel(nullptr)
    , m_ocrHotkey(nullptr)
    , m_secondaryLanguage(QStringLiteral("en"))
    , m_quitRequested(false)
    , m_ocrInProgress(false)
    , m_darkTheme(false)
    , m_manualTranslationOverride(false)
    , m_updatingLanguageCombos(false)
{
    ui->setupUi(this);
    m_lightStyleSheet = styleSheet();
    setWindowIcon(QIcon(":/icons/app.ico"));
    ui->appIconLabel->setPixmap(atlasHubWindowIconPixmap(false, 54));
    ui->btnRunOCR->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    ui->btnTranslate->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));

    setupTrayIcon();
    setupCopyButtons();
    setupTextToolbar();
    setupHelp();
    setupLanguages();
    setupUiLanguageMenu();
    setupSettingsActions();
    setupHistory();
    setupTechnicalStatusBar();
    loadUiLanguageSettings();
    loadSmartLanguageSettings();
    resetAutomaticTranslationMode();
    loadHistory();

    ui->errorLog->setPlainText(tr("Pronto para capturar OCR."));
    statusBar()->showMessage(tr("Pronto."));
    applyTheme();
    setupHotkey();

    connect(ui->btnRunOCR, &QPushButton::clicked, this, [this]() {
        startOcrCapture(true);
    });

    connect(ui->btnTranslate, &QPushButton::clicked, this, [this]() {
        runTranslation(false);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::restoreFromTray()
{
    setWindowState(windowState() & ~Qt::WindowMinimized);
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_quitRequested) {
        event->accept();
        return;
    }

    event->ignore();
    resetAutomaticTranslationMode();
    hide();
    showTrayCloseMessage();
}

void MainWindow::appendError(const QString &message)
{
    statusBar()->showMessage(message, 6000);

    if (ui->errorLog->toPlainText().isEmpty()) {
        ui->errorLog->setPlainText(message);
        return;
    }

    ui->errorLog->appendPlainText(message);
}

void MainWindow::applyTextColor(const QColor &color)
{
    QTextCharFormat format;
    format.setForeground(color);
    mergeOutputFormat(format);
}

void MainWindow::applyTheme()
{
    QSettings settings;
    m_darkTheme = settings.value(QStringLiteral("ui/darkTheme"), false).toBool();

    setStyleSheet(m_darkTheme ? darkThemeStyleSheet() : m_lightStyleSheet);
    ui->appIconLabel->setPixmap(atlasHubWindowIconPixmap(m_darkTheme, 54));

    const QList<QLabel *> technicalLabels{
        m_systemStatusLabel,
        m_engineStatusLabel,
        m_ocrStatusLabel,
        m_translationStatusLabel,
    };

    for (QLabel *label : technicalLabels) {
        if (label) {
            label->setVisible(m_darkTheme);
        }
    }

    if (m_trayMenu) {
        m_trayMenu->setStyleSheet(trayMenuStyleSheet(m_darkTheme));
    }
}

void MainWindow::applyUiLanguage(const QString &localeName)
{
    const QString requestedLanguage = localeName.isEmpty() ? QStringLiteral("pt_BR") : localeName;
    qApp->removeTranslator(&m_translator);

    if (requestedLanguage != QStringLiteral("pt_BR")) {
        const QString resourcePath = QStringLiteral(":/i18n/atlashub_%1.qm").arg(requestedLanguage);
        bool loaded = m_translator.load(resourcePath);

        if (!loaded) {
            const QString filePath = QDir(QCoreApplication::applicationDirPath())
                                         .filePath(QStringLiteral("translations/atlashub_%1.qm")
                                                       .arg(requestedLanguage));
            loaded = m_translator.load(filePath);
        }

        if (loaded) {
            qApp->installTranslator(&m_translator);
        }
    }

    m_uiLanguage = requestedLanguage;
    saveUiLanguageSettings();
    ui->retranslateUi(this);
    retranslateDynamicUi();
    refreshHistoryView();
}

void MainWindow::addHistoryEntry(const QString &ocrText, const QString &translationText)
{
    const QString trimmedOcr = ocrText.trimmed();
    const QString trimmedTranslation = translationText.trimmed();
    if (trimmedOcr.isEmpty() && trimmedTranslation.isEmpty()) {
        return;
    }

    if (!m_history.isEmpty()
        && m_history.first().ocrText == trimmedOcr
        && m_history.first().translationText == trimmedTranslation) {
        return;
    }

    HistoryEntry entry;
    entry.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    entry.ocrText = trimmedOcr;
    entry.translationText = trimmedTranslation;
    entry.favorite = false;
    m_history.prepend(entry);

    while (m_history.size() > 50) {
        int removeIndex = -1;
        for (int i = m_history.size() - 1; i >= 0; --i) {
            if (!m_history.at(i).favorite) {
                removeIndex = i;
                break;
            }
        }

        m_history.removeAt(removeIndex >= 0 ? removeIndex : m_history.size() - 1);
    }

    saveHistory();
    refreshHistoryView();
}

void MainWindow::copyTextToClipboard(const QString &text,
                                     const QString &emptyMessage,
                                     const QString &successMessage)
{
    const QString trimmedText = text.trimmed();
    if (trimmedText.isEmpty()) {
        appendError(emptyMessage);
        return;
    }

    QApplication::clipboard()->setText(trimmedText);
    appendError(successMessage);
}

QString MainWindow::currentApplicationPath() const
{
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
}

QString MainWindow::defaultPrimaryLanguageCode() const
{
    if (m_uiLanguage.startsWith(QStringLiteral("en"), Qt::CaseInsensitive)) {
        return QStringLiteral("en");
    }

    if (m_uiLanguage.startsWith(QStringLiteral("fr"), Qt::CaseInsensitive)) {
        return QStringLiteral("fr");
    }

    if (m_uiLanguage.startsWith(QStringLiteral("es"), Qt::CaseInsensitive)) {
        return QStringLiteral("es");
    }

    return QStringLiteral("pt");
}

bool MainWindow::historyEntryMatchesFilters(const HistoryEntry &entry) const
{
    if (m_historyFavoritesOnlyCheck && m_historyFavoritesOnlyCheck->isChecked() && !entry.favorite) {
        return false;
    }

    if (m_historySearchEdit) {
        const QString searchText = m_historySearchEdit->text().trimmed();
        if (!searchText.isEmpty()
            && !entry.ocrText.contains(searchText, Qt::CaseInsensitive)
            && !entry.translationText.contains(searchText, Qt::CaseInsensitive)) {
            return false;
        }
    }

    if (!m_historyDateFilterCombo) {
        return true;
    }

    const int daysBack = m_historyDateFilterCombo->currentData().toInt();
    if (daysBack <= 0) {
        return true;
    }

    const QDateTime timestamp = QDateTime::fromString(entry.timestamp, Qt::ISODate);
    if (!timestamp.isValid()) {
        return false;
    }

    return timestamp.date() >= QDate::currentDate().addDays(-(daysBack - 1));
}

bool MainWindow::isWindowsStartupEnabled() const
{
#ifdef Q_OS_WIN
    QSettings runKey(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                     QSettings::NativeFormat);
    return runKey.value(QStringLiteral("AtlasHub")).toString().contains(currentApplicationPath(),
                                                                        Qt::CaseInsensitive);
#else
    return false;
#endif
}

bool MainWindow::shouldStartMinimizedToTray() const
{
    QSettings settings;
    return settings.value(QStringLiteral("startup/startMinimizedToTray"), false).toBool();
}

void MainWindow::handleOcrFinished(const QString &path, bool restoreWindowWhenFinished)
{
    if (path.isEmpty() || !QFile::exists(path)) {
        appendError(tr("Falha ao capturar a área selecionada."));
        setTechnicalStatus(QStringLiteral("ERROR"), QString());
        m_ocrInProgress = false;
        if (restoreWindowWhenFinished) {
            restoreFromTray();
        }
        return;
    }

    appendError(tr("Imagem capturada. Executando OCR..."));
    setTechnicalStatus(QStringLiteral("READ"), QString());
    QCoreApplication::processEvents();

    OCRService ocrService;
    const QString result = ocrService.extractText(path, ui->sourceLanguageCombo->currentData().toString());
    const QString visibleResult = result.trimmed();

    if (visibleResult.isEmpty()) {
        appendError(tr("OCR concluído, mas nenhum texto foi reconhecido."));
        setTechnicalStatus(QStringLiteral("IDLE"), QString());
        m_ocrInProgress = false;
        if (restoreWindowWhenFinished) {
            restoreFromTray();
        }
        return;
    }

    if (isErrorMessage(visibleResult)) {
        appendError(visibleResult);
        setTechnicalStatus(QStringLiteral("ERROR"), QString());
        m_ocrInProgress = false;
        if (restoreWindowWhenFinished) {
            restoreFromTray();
        }
        return;
    }

    appendError(tr("OCR concluído."));
    setTechnicalStatus(QStringLiteral("IDLE"), QString());
    ui->textOutput->setPlainText(result);
    m_ocrInProgress = false;
    if (restoreWindowWhenFinished) {
        restoreFromTray();
    }
    ui->textOutput->setFocus();
    runTranslation(true);
}

QString MainWindow::ocrHotkeyText() const
{
    QSettings settings;
    return settings.value(QStringLiteral("hotkey/ocr"),
                          defaultOcrHotkey().toString(QKeySequence::PortableText))
        .toString();
}

bool MainWindow::registerOcrHotkey(const QKeySequence &shortcut, bool persist)
{
    if (!m_ocrHotkey || shortcut.isEmpty()) {
        return false;
    }

    if (m_ocrHotkey->registerShortcut(shortcut)) {
        m_ocrHotkeyText = shortcut.toString(QKeySequence::PortableText);

        if (persist) {
            QSettings settings;
            settings.setValue(QStringLiteral("hotkey/ocr"), m_ocrHotkeyText);
        }

        return true;
    }

    const QString errorDetails = m_ocrHotkey->lastError();
    appendError(tr("Falha ao registrar atalho global: %1%2")
                    .arg(shortcut.toString(QKeySequence::NativeText),
                         errorDetails.isEmpty() ? QString() : QStringLiteral(" - ") + errorDetails));
    return false;
}

void MainWindow::runTranslation(bool useSmartTarget)
{
    const QString text = ui->textOutput->toPlainText();
    if (text.trimmed().isEmpty()) {
        appendError(tr("Falha: não há texto de OCR para traduzir."));
        setTechnicalStatus(QString(), QStringLiteral("ERROR"));
        return;
    }

    ui->btnTranslate->setEnabled(false);
    ui->translationOutput->setPlainText(tr("Traduzindo..."));
    setTechnicalStatus(QString(), QStringLiteral("RUN"));

    const QString fallbackSourceLanguage = ui->sourceLanguageCombo->currentData(Qt::UserRole + 1).toString();
    const QString selectedTargetLanguage = ui->targetLanguageCombo->currentData(Qt::UserRole + 1).toString();
    const QString primaryLanguage = m_primaryLanguage.isEmpty() ? defaultPrimaryLanguageCode() : m_primaryLanguage;
    const QString secondaryLanguage = m_secondaryLanguage.isEmpty() ? QStringLiteral("en") : m_secondaryLanguage;
    const bool smartTargetEnabled = useSmartTarget && !m_manualTranslationOverride;
    const bool detectLanguage = smartTargetEnabled || !m_manualTranslationOverride;
    auto *thread = QThread::create([text,
                                    fallbackSourceLanguage,
                                    selectedTargetLanguage,
                                    primaryLanguage,
                                    secondaryLanguage,
                                    smartTargetEnabled,
                                    detectLanguage,
                                    window = QPointer<MainWindow>(this)]() {
        TranslationService translationService;
        TranslationResult result;
        result.sourceText = text;
        result.detectedLanguage = detectLanguage ? translationService.detectLanguage(text) : QString();
        result.sourceLanguage = result.detectedLanguage.trimmed().isEmpty()
                                    ? fallbackSourceLanguage
                                    : result.detectedLanguage;
        result.targetLanguage = selectedTargetLanguage;

        if (smartTargetEnabled) {
            if (result.detectedLanguage == primaryLanguage) {
                result.targetLanguage = secondaryLanguage;
            } else if (result.detectedLanguage == secondaryLanguage) {
                result.targetLanguage = primaryLanguage;
            }
        }

        result.translatedText = translationService.translateText(text, result.sourceLanguage, result.targetLanguage);

        if (!window) {
            return;
        }

        QMetaObject::invokeMethod(window,
                                  [window, result]() {
                                      if (!window) {
                                          return;
                                      }

                                       if (window->ui->textOutput->toPlainText() != result.sourceText) {
                                           window->ui->btnTranslate->setEnabled(true);
                                           window->setTechnicalStatus(QString(), QStringLiteral("IDLE"));
                                           return;
                                       }

                                      const int detectedLanguageIndex =
                                          window->sourceLanguageIndexForTranslateCode(result.sourceLanguage);
                                      if (detectedLanguageIndex >= 0
                                          && detectedLanguageIndex != window->ui->sourceLanguageCombo->currentIndex()) {
                                          window->m_updatingLanguageCombos = true;
                                          window->ui->sourceLanguageCombo->setCurrentIndex(detectedLanguageIndex);
                                          window->m_updatingLanguageCombos = false;
                                          window->appendError(
                                              QCoreApplication::translate("MainWindow",
                                                                          "Idioma detectado automaticamente: %1.")
                                                  .arg(window->ui->sourceLanguageCombo->currentText()));
                                      }

                                      const int targetLanguageIndex =
                                          window->languageIndexForTranslateCode(window->ui->targetLanguageCombo,
                                                                                result.targetLanguage);
                                      if (targetLanguageIndex >= 0
                                          && targetLanguageIndex != window->ui->targetLanguageCombo->currentIndex()) {
                                          window->m_updatingLanguageCombos = true;
                                          window->ui->targetLanguageCombo->setCurrentIndex(targetLanguageIndex);
                                          window->m_updatingLanguageCombos = false;
                                      }

                                      window->ui->btnTranslate->setEnabled(true);

                                       if (isErrorMessage(result.translatedText.trimmed())) {
                                           window->ui->translationOutput->clear();
                                           window->appendError(result.translatedText.trimmed());
                                           window->setTechnicalStatus(QString(), QStringLiteral("ERROR"));
                                           return;
                                       }

                                       window->ui->translationOutput->setPlainText(result.translatedText);
                                       window->appendError(QCoreApplication::translate("MainWindow",
                                                                                       "Tradução concluída."));
                                       window->setTechnicalStatus(QString(), QStringLiteral("IDLE"));
                                       window->addHistoryEntry(result.sourceText, result.translatedText);
                                  },
                                  Qt::QueuedConnection);
    });

    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

int MainWindow::languageIndexForTranslateCode(QComboBox *combo, const QString &languageCode) const
{
    if (!combo) {
        return -1;
    }

    for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemData(i, Qt::UserRole + 1).toString() == languageCode) {
            return i;
        }
    }

    return -1;
}

int MainWindow::sourceLanguageIndexForTranslateCode(const QString &languageCode) const
{
    return languageIndexForTranslateCode(ui->sourceLanguageCombo, languageCode);
}

void MainWindow::loadSmartLanguageSettings()
{
    QSettings settings;
    m_primaryLanguage = settings.value(QStringLiteral("translation/primaryLanguage"),
                                       defaultPrimaryLanguageCode())
                            .toString();
    m_secondaryLanguage = settings.value(QStringLiteral("translation/secondaryLanguage"),
                                         QStringLiteral("en"))
                              .toString();

    if (m_primaryLanguage.trimmed().isEmpty()) {
        m_primaryLanguage = defaultPrimaryLanguageCode();
    }

    if (m_secondaryLanguage.trimmed().isEmpty()) {
        m_secondaryLanguage = QStringLiteral("en");
    }
}

void MainWindow::loadTranslationLanguageSettings()
{
    QSettings settings;
    const QString sourceLanguage = settings.value(QStringLiteral("translation/sourceLanguage"),
                                                  QStringLiteral("pt")).toString();
    const QString targetLanguage = settings.value(QStringLiteral("translation/targetLanguage"),
                                                  QStringLiteral("en")).toString();

    const int sourceIndex = sourceLanguageIndexForTranslateCode(sourceLanguage);
    if (sourceIndex >= 0) {
        ui->sourceLanguageCombo->setCurrentIndex(sourceIndex);
    }

    for (int i = 0; i < ui->targetLanguageCombo->count(); ++i) {
        if (ui->targetLanguageCombo->itemData(i, Qt::UserRole + 1).toString() == targetLanguage) {
            ui->targetLanguageCombo->setCurrentIndex(i);
            break;
        }
    }
}

void MainWindow::loadUiLanguageSettings()
{
    QSettings settings;
    applyUiLanguage(settings.value(QStringLiteral("ui/language"), QStringLiteral("pt_BR")).toString());
}

void MainWindow::loadHistory()
{
    QSettings settings;
    const QByteArray rawHistory = settings.value(QStringLiteral("history/items")).toByteArray();
    const QJsonDocument document = QJsonDocument::fromJson(rawHistory);
    m_history.clear();

    for (const QJsonValue &value : document.array()) {
        const QJsonObject object = value.toObject();
        HistoryEntry entry;
        entry.timestamp = object.value(QStringLiteral("timestamp")).toString();
        entry.ocrText = object.value(QStringLiteral("ocr")).toString();
        entry.translationText = object.value(QStringLiteral("translation")).toString();
        entry.favorite = object.value(QStringLiteral("favorite")).toBool(false);
        if (!entry.ocrText.trimmed().isEmpty() || !entry.translationText.trimmed().isEmpty()) {
            m_history.append(entry);
        }
    }

    refreshHistoryView();
}

void MainWindow::refreshHistoryView()
{
    if (!m_historyList) {
        return;
    }

    m_historyList->clear();
    int visibleCount = 0;

    for (int i = 0; i < m_history.size(); ++i) {
        const HistoryEntry &entry = m_history.at(i);
        if (!historyEntryMatchesFilters(entry)) {
            continue;
        }

        const QDateTime timestamp = QDateTime::fromString(entry.timestamp, Qt::ISODate);
        QString title = timestamp.isValid()
                            ? timestamp.toString(QStringLiteral("dd/MM HH:mm"))
                            : tr("Captura");
        if (entry.favorite) {
            title.prepend(QStringLiteral("* "));
        }

        const QString previewSource = entry.translationText.isEmpty() ? entry.ocrText : entry.translationText;
        QString preview = previewSource.simplified();
        if (preview.size() > 76) {
            preview = preview.left(73) + QStringLiteral("...");
        }

        auto *item = new QListWidgetItem(QStringLiteral("%1  %2").arg(title, preview), m_historyList);
        item->setData(Qt::UserRole, i);
        item->setToolTip(tr("Clique duas vezes para restaurar esta captura."));
        ++visibleCount;
    }

    if (visibleCount == 0 && !m_history.isEmpty()) {
        auto *item = new QListWidgetItem(tr("Nenhuma captura encontrada com os filtros atuais."), m_historyList);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setData(Qt::UserRole, -1);
    }
}

void MainWindow::resetAutomaticTranslationMode()
{
    m_manualTranslationOverride = false;

    const int sourceIndex = languageIndexForTranslateCode(ui->sourceLanguageCombo, m_primaryLanguage);
    const int targetIndex = languageIndexForTranslateCode(ui->targetLanguageCombo, m_secondaryLanguage);

    m_updatingLanguageCombos = true;
    if (sourceIndex >= 0) {
        ui->sourceLanguageCombo->setCurrentIndex(sourceIndex);
    }
    if (targetIndex >= 0) {
        ui->targetLanguageCombo->setCurrentIndex(targetIndex);
    }
    m_updatingLanguageCombos = false;
}

void MainWindow::saveTranslationLanguageSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("translation/sourceLanguage"),
                      ui->sourceLanguageCombo->currentData(Qt::UserRole + 1).toString());
    settings.setValue(QStringLiteral("translation/targetLanguage"),
                      ui->targetLanguageCombo->currentData(Qt::UserRole + 1).toString());
}

void MainWindow::saveUiLanguageSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/language"), m_uiLanguage);
}

void MainWindow::saveHistory() const
{
    QJsonArray array;
    for (const HistoryEntry &entry : m_history) {
        QJsonObject object;
        object.insert(QStringLiteral("timestamp"), entry.timestamp);
        object.insert(QStringLiteral("ocr"), entry.ocrText);
        object.insert(QStringLiteral("translation"), entry.translationText);
        object.insert(QStringLiteral("favorite"), entry.favorite);
        array.append(object);
    }

    QSettings settings;
    settings.setValue(QStringLiteral("history/items"), QJsonDocument(array).toJson(QJsonDocument::Compact));
}

void MainWindow::saveSmartLanguageSettings() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("translation/primaryLanguage"), m_primaryLanguage);
    settings.setValue(QStringLiteral("translation/secondaryLanguage"), m_secondaryLanguage);
}

void MainWindow::setWindowsStartupEnabled(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings runKey(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                     QSettings::NativeFormat);
    if (enabled) {
        runKey.setValue(QStringLiteral("AtlasHub"),
                        QStringLiteral("\"%1\"").arg(currentApplicationPath()));
        return;
    }

    runKey.remove(QStringLiteral("AtlasHub"));
#else
    Q_UNUSED(enabled);
#endif
}

void MainWindow::mergeOutputFormat(const QTextCharFormat &format)
{
    QTextCursor cursor = ui->textOutput->textCursor();
    if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::Document);
    }

    cursor.mergeCharFormat(format);
    ui->textOutput->mergeCurrentCharFormat(format);
    ui->textOutput->setTextCursor(cursor);
    ui->textOutput->setFocus();
}

void MainWindow::refreshTranslationLanguageLabels()
{
    QString sourceLanguage = ui->sourceLanguageCombo->currentData(Qt::UserRole + 1).toString();
    QString targetLanguage = ui->targetLanguageCombo->currentData(Qt::UserRole + 1).toString();

    if (sourceLanguage.isEmpty()) {
        sourceLanguage = QStringLiteral("pt");
    }

    if (targetLanguage.isEmpty()) {
        targetLanguage = QStringLiteral("en");
    }

    const QSignalBlocker sourceBlocker(ui->sourceLanguageCombo);
    const QSignalBlocker targetBlocker(ui->targetLanguageCombo);

    ui->sourceLanguageCombo->clear();
    ui->targetLanguageCombo->clear();

    for (const LanguageOption &language : languageOptions) {
        ui->sourceLanguageCombo->addItem(tr(language.label), language.ocrCode);
        ui->sourceLanguageCombo->setItemData(ui->sourceLanguageCombo->count() - 1,
                                             language.translateCode,
                                             Qt::UserRole + 1);

        ui->targetLanguageCombo->addItem(tr(language.label), language.ocrCode);
        ui->targetLanguageCombo->setItemData(ui->targetLanguageCombo->count() - 1,
                                             language.translateCode,
                                             Qt::UserRole + 1);
    }

    const int sourceIndex = sourceLanguageIndexForTranslateCode(sourceLanguage);
    if (sourceIndex >= 0) {
        ui->sourceLanguageCombo->setCurrentIndex(sourceIndex);
    }

    for (int i = 0; i < ui->targetLanguageCombo->count(); ++i) {
        if (ui->targetLanguageCombo->itemData(i, Qt::UserRole + 1).toString() == targetLanguage) {
            ui->targetLanguageCombo->setCurrentIndex(i);
            break;
        }
    }
}

void MainWindow::retranslateDynamicUi()
{
    refreshTranslationLanguageLabels();

    ui->actionHelp->setText(tr("Ajuda"));
    ui->actionHelp->setToolTip(tr("Abrir ajuda"));

    if (m_formatMenu) {
        m_formatMenu->setTitle(tr("Formatação"));
    }

    if (m_uiLanguageMenu) {
        m_uiLanguageMenu->setTitle(tr("Idioma da interface"));
    }

    if (m_formatToolbar) {
        m_formatToolbar->setWindowTitle(tr("Formatação"));
    }

    if (m_fontBox) {
        m_fontBox->setToolTip(tr("Fonte"));
    }

    if (m_sizeBox) {
        m_sizeBox->setToolTip(tr("Tamanho"));
        m_sizeBox->setSuffix(tr(" pt"));
    }

    if (m_boldAction) {
        m_boldAction->setText(tr("Negrito"));
        m_boldAction->setIconText(tr("B"));
        m_boldAction->setToolTip(tr("Negrito"));
    }

    if (m_italicAction) {
        m_italicAction->setText(tr("Itálico"));
        m_italicAction->setIconText(tr("I"));
        m_italicAction->setToolTip(tr("Itálico"));
    }

    if (m_underlineAction) {
        m_underlineAction->setText(tr("Sublinhado"));
        m_underlineAction->setIconText(tr("U"));
        m_underlineAction->setToolTip(tr("Sublinhado"));
    }

    if (m_colorAction) {
        m_colorAction->setText(tr("Cor do texto"));
        m_colorAction->setIconText(tr("Cor"));
        m_colorAction->setToolTip(tr("Cor do texto"));
    }

    if (m_portugueseAction) {
        m_portugueseAction->setText(tr("Português", "UI language option"));
    }

    if (m_englishAction) {
        m_englishAction->setText(tr("English", "UI language option"));
    }

    if (m_frenchAction) {
        m_frenchAction->setText(tr("Français", "UI language option"));
    }

    if (m_openAtlasHubAction) {
        m_openAtlasHubAction->setText(tr("Abrir AtlasHub"));
    }

    if (m_runOcrAction) {
        m_runOcrAction->setText(tr("Executar OCR"));
    }

    if (m_quitAction) {
        m_quitAction->setText(tr("Sair"));
    }

    if (m_configureHotkeyAction) {
        m_configureHotkeyAction->setText(tr("Atalho OCR..."));
    }

    if (m_startWithWindowsAction) {
        m_startWithWindowsAction->setText(tr("Inicializar com Windows"));
    }

    if (m_startMinimizedAction) {
        m_startMinimizedAction->setText(tr("Iniciar minimizado na tray"));
    }

    if (m_darkThemeAction) {
        m_darkThemeAction->setText(tr("Tema escuro"));
    }

    if (m_smartLanguagesAction) {
        m_smartLanguagesAction->setText(tr("Idiomas inteligentes do OCR..."));
    }

    if (m_clearHistoryAction) {
        m_clearHistoryAction->setText(tr("Limpar histórico"));
    }

    if (m_copyOcrButton) {
        m_copyOcrButton->setToolTip(tr("Copiar texto OCR"));
    }

    if (m_copyTranslationButton) {
        m_copyTranslationButton->setToolTip(tr("Copiar tradução"));
    }

    if (m_historyDock) {
        m_historyDock->setWindowTitle(tr("Histórico"));
    }

    if (m_historySearchEdit) {
        m_historySearchEdit->setPlaceholderText(tr("Buscar no histórico"));
    }

    if (m_historyDateFilterCombo) {
        const QSignalBlocker blocker(m_historyDateFilterCombo);
        const int currentIndex = m_historyDateFilterCombo->currentIndex();
        m_historyDateFilterCombo->setItemText(0, tr("Todas as datas"));
        m_historyDateFilterCombo->setItemText(1, tr("Hoje"));
        m_historyDateFilterCombo->setItemText(2, tr("Últimos 7 dias"));
        m_historyDateFilterCombo->setItemText(3, tr("Últimos 30 dias"));
        m_historyDateFilterCombo->setCurrentIndex(currentIndex);
    }

    if (m_historyFavoritesOnlyCheck) {
        m_historyFavoritesOnlyCheck->setText(tr("Favoritos"));
    }

    if (m_historyFavoriteButton) {
        m_historyFavoriteButton->setToolTip(tr("Favoritar captura selecionada"));
    }

    syncUiLanguageActions();
}

void MainWindow::setupCopyButtons()
{
    m_copyOcrButton = ui->copyOcrButton;
    m_copyOcrButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    m_copyOcrButton->setToolTip(tr("Copiar texto OCR"));

    m_copyTranslationButton = ui->copyTranslationButton;
    m_copyTranslationButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    m_copyTranslationButton->setToolTip(tr("Copiar tradução"));

    connect(m_copyOcrButton, &QToolButton::clicked, this, [this]() {
        copyTextToClipboard(ui->textOutput->toPlainText(),
                            tr("Não há texto OCR para copiar."),
                            tr("Texto OCR copiado."));
    });

    connect(m_copyTranslationButton, &QToolButton::clicked, this, [this]() {
        copyTextToClipboard(ui->translationOutput->toPlainText(),
                            tr("Não há tradução para copiar."),
                            tr("Tradução copiada."));
    });
}

void MainWindow::setupHelp()
{
    ui->actionHelp->setText(tr("Ajuda"));
    ui->actionHelp->setToolTip(tr("Abrir ajuda"));
    connect(ui->actionHelp, &QAction::triggered, this, &MainWindow::showHelpWindow);
}

void MainWindow::setupHistory()
{
    m_historyDock = ui->historyDock;
    m_historyDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_historyDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_historyDock->setFloating(false);

    m_historySearchEdit = ui->historySearchEdit;
    m_historyDateFilterCombo = ui->historyDateFilterCombo;
    m_historyFavoritesOnlyCheck = ui->historyFavoritesOnlyCheck;
    m_historyFavoriteButton = ui->historyFavoriteButton;
    m_historyList = ui->historyList;

    m_historyDateFilterCombo->clear();
    m_historyDateFilterCombo->addItem(tr("Todas as datas"), 0);
    m_historyDateFilterCombo->addItem(tr("Hoje"), 1);
    m_historyDateFilterCombo->addItem(tr("Últimos 7 dias"), 7);
    m_historyDateFilterCombo->addItem(tr("Últimos 30 dias"), 30);

    m_historyFavoriteButton->setToolTip(tr("Favoritar captura selecionada"));
    m_historyFavoriteButton->setEnabled(false);

    connect(m_historySearchEdit, &QLineEdit::textChanged, this, &MainWindow::refreshHistoryView);
    connect(m_historyDateFilterCombo, &QComboBox::currentIndexChanged, this, [this]() {
        refreshHistoryView();
    });
    connect(m_historyFavoritesOnlyCheck, &QCheckBox::toggled, this, &MainWindow::refreshHistoryView);
    connect(m_historyFavoriteButton, &QToolButton::clicked, this, &MainWindow::toggleSelectedHistoryFavorite);
    connect(m_historyList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current) {
        const bool hasHistoryItem = current && current->data(Qt::UserRole).toInt() >= 0;
        m_historyFavoriteButton->setEnabled(hasHistoryItem);

        if (!hasHistoryItem) {
            m_historyFavoriteButton->setToolTip(tr("Favoritar captura selecionada"));
            return;
        }

        const int index = current->data(Qt::UserRole).toInt();
        m_historyFavoriteButton->setToolTip(m_history.at(index).favorite
                                                ? tr("Remover dos favoritos")
                                                : tr("Favoritar captura selecionada"));
    });

    connect(m_historyList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        const int index = item->data(Qt::UserRole).toInt();
        if (index < 0 || index >= m_history.size()) {
            return;
        }

        const HistoryEntry &entry = m_history.at(index);
        ui->textOutput->setPlainText(entry.ocrText);
        ui->translationOutput->setPlainText(entry.translationText);
        appendError(tr("Histórico restaurado."));
    });
}

void MainWindow::setupHotkey()
{
    m_ocrHotkey = new GlobalHotkey(this);
    m_ocrHotkey->setNativeWindowId(reinterpret_cast<quintptr>(winId()));

    connect(m_ocrHotkey, &GlobalHotkey::activated, this, [this]() {
        startOcrCapture(true);
    });

    const QKeySequence configuredShortcut = QKeySequence::fromString(ocrHotkeyText(),
                                                                     QKeySequence::PortableText);
    const QKeySequence shortcut = configuredShortcut.isEmpty() ? defaultOcrHotkey() : configuredShortcut;
    registerOcrHotkey(shortcut, false);
}

void MainWindow::setupLanguages()
{
    refreshTranslationLanguageLabels();
    loadTranslationLanguageSettings();

    connect(ui->sourceLanguageCombo, &QComboBox::currentIndexChanged, this, [this]() {
        if (m_updatingLanguageCombos) {
            return;
        }
        m_manualTranslationOverride = true;
    });

    connect(ui->targetLanguageCombo, &QComboBox::currentIndexChanged, this, [this]() {
        if (m_updatingLanguageCombos) {
            return;
        }
        m_manualTranslationOverride = true;
    });
}

void MainWindow::setupTechnicalStatusBar()
{
    statusBar()->setSizeGripEnabled(false);

    m_systemStatusLabel = new QLabel(QStringLiteral("●  SYSTEM READY"), this);
    m_systemStatusLabel->setObjectName(QStringLiteral("systemStatusLabel"));

    m_engineStatusLabel = new QLabel(QStringLiteral("TESSERACT v5.3"), this);
    m_engineStatusLabel->setObjectName(QStringLiteral("engineStatusLabel"));

    m_ocrStatusLabel = new QLabel(QStringLiteral("OCR: IDLE"), this);
    m_ocrStatusLabel->setObjectName(QStringLiteral("ocrStatusLabel"));

    m_translationStatusLabel = new QLabel(QStringLiteral("TRANS: IDLE"), this);
    m_translationStatusLabel->setObjectName(QStringLiteral("translationStatusLabel"));

    statusBar()->addPermanentWidget(m_systemStatusLabel);
    statusBar()->addPermanentWidget(m_engineStatusLabel);
    statusBar()->addPermanentWidget(m_ocrStatusLabel);
    statusBar()->addPermanentWidget(m_translationStatusLabel);
}

void MainWindow::setTechnicalStatus(const QString &ocrStatus, const QString &translationStatus)
{
    if (m_ocrStatusLabel && !ocrStatus.isEmpty()) {
        m_ocrStatusLabel->setText(QStringLiteral("OCR: ") + ocrStatus);
    }

    if (m_translationStatusLabel && !translationStatus.isEmpty()) {
        m_translationStatusLabel->setText(QStringLiteral("TRANS: ") + translationStatus);
    }
}

void MainWindow::setupTrayIcon()
{
    m_trayMenu = new QMenu(this);
    m_trayMenu->setStyleSheet(trayMenuStyleSheet(m_darkTheme));

    m_openAtlasHubAction = m_trayMenu->addAction(tr("Abrir AtlasHub"));
    m_runOcrAction = m_trayMenu->addAction(tr("Executar OCR"));
    m_trayMenu->addAction(tr("Copiar OCR"), this, [this]() {
        copyTextToClipboard(ui->textOutput->toPlainText(),
                            tr("Não há texto OCR para copiar."),
                            tr("Texto OCR copiado."));
    });
    m_trayMenu->addAction(tr("Copiar tradução"), this, [this]() {
        copyTextToClipboard(ui->translationOutput->toPlainText(),
                            tr("Não há tradução para copiar."),
                            tr("Tradução copiada."));
    });
    m_trayMenu->addSeparator();
    m_quitAction = m_trayMenu->addAction(tr("Sair"));

    m_trayIcon = new QSystemTrayIcon(QIcon(":/icons/app.ico"), this);
    m_trayIcon->setToolTip(QStringLiteral("AtlasHub"));
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->show();

    connect(m_openAtlasHubAction, &QAction::triggered, this, &MainWindow::restoreFromTray);
    connect(m_runOcrAction, &QAction::triggered, this, [this]() {
        startOcrCapture(true);
    });
    connect(m_quitAction, &QAction::triggered, this, &MainWindow::quitFromTray);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            restoreFromTray();
        }
    });
}

void MainWindow::setupSettingsActions()
{
    QSettings settings;

    m_startWithWindowsAction = ui->actionStartWithWindows;
    m_startWithWindowsAction->setChecked(isWindowsStartupEnabled());

    m_startMinimizedAction = ui->actionStartMinimized;
    m_startMinimizedAction->setChecked(settings.value(QStringLiteral("startup/startMinimizedToTray"), false).toBool());

    m_darkThemeAction = ui->actionDarkTheme;
    m_darkThemeAction->setChecked(settings.value(QStringLiteral("ui/darkTheme"), false).toBool());

    m_smartLanguagesAction = ui->actionSmartLanguages;
    m_clearHistoryAction = ui->actionClearHistory;

    connect(m_startWithWindowsAction, &QAction::toggled, this, [this](bool enabled) {
        setWindowsStartupEnabled(enabled);
        appendError(enabled ? tr("AtlasHub será iniciado com o Windows.")
                            : tr("Inicialização com Windows desativada."));
    });

    connect(m_startMinimizedAction, &QAction::toggled, this, [this](bool enabled) {
        QSettings settings;
        settings.setValue(QStringLiteral("startup/startMinimizedToTray"), enabled);
        appendError(enabled ? tr("AtlasHub iniciará minimizado na tray.")
                            : tr("AtlasHub abrirá a janela ao iniciar."));
    });

    connect(m_darkThemeAction, &QAction::toggled, this, [this](bool enabled) {
        QSettings settings;
        settings.setValue(QStringLiteral("ui/darkTheme"), enabled);
        applyTheme();
        appendError(enabled ? tr("Tema escuro ativado.") : tr("Tema claro ativado."));
    });

    connect(m_smartLanguagesAction, &QAction::triggered, this, &MainWindow::showSmartLanguageSettingsDialog);

    connect(m_clearHistoryAction, &QAction::triggered, this, [this]() {
        m_history.clear();
        saveHistory();
        refreshHistoryView();
        appendError(tr("Histórico limpo."));
    });
}

void MainWindow::setupTextToolbar()
{
    m_formatMenu = ui->menuFormatacao;
    m_boldAction = ui->actionBold;
    m_boldAction->setToolTip(tr("Negrito"));

    m_italicAction = ui->actionItalic;
    m_italicAction->setToolTip(tr("Itálico"));

    m_underlineAction = ui->actionUnderline;
    m_underlineAction->setToolTip(tr("Sublinhado"));

    m_colorAction = ui->actionTextColor;
    m_colorAction->setToolTip(tr("Cor do texto"));

    connect(m_boldAction, &QAction::toggled, this, [this](bool checked) {
        QTextCharFormat format;
        format.setFontWeight(checked ? QFont::Bold : QFont::Normal);
        mergeOutputFormat(format);
    });

    connect(m_italicAction, &QAction::toggled, this, [this](bool checked) {
        QTextCharFormat format;
        format.setFontItalic(checked);
        mergeOutputFormat(format);
    });

    connect(m_underlineAction, &QAction::toggled, this, [this](bool checked) {
        QTextCharFormat format;
        format.setFontUnderline(checked);
        mergeOutputFormat(format);
    });

    connect(m_colorAction, &QAction::triggered, this, [this]() {
        const QColor color = QColorDialog::getColor(ui->textOutput->textColor(),
                                                     this,
                                                     tr("Escolher cor do texto"));
        if (color.isValid()) {
            applyTextColor(color);
        }
    });
}

void MainWindow::setupUiLanguageMenu()
{
    m_uiLanguageMenu = ui->menuUiLanguage;
    m_uiLanguageGroup = new QActionGroup(this);
    m_uiLanguageGroup->setExclusive(true);

    m_portugueseAction = ui->actionPortuguese;
    m_portugueseAction->setData(QStringLiteral("pt_BR"));
    m_uiLanguageGroup->addAction(m_portugueseAction);

    m_englishAction = ui->actionEnglish;
    m_englishAction->setData(QStringLiteral("en_US"));
    m_uiLanguageGroup->addAction(m_englishAction);

    m_frenchAction = ui->actionFrench;
    m_frenchAction->setData(QStringLiteral("fr_FR"));
    m_uiLanguageGroup->addAction(m_frenchAction);

    connect(m_uiLanguageGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        applyUiLanguage(action->data().toString());
    });

    m_configureHotkeyAction = ui->actionConfigureHotkey;
    connect(m_configureHotkeyAction, &QAction::triggered, this, &MainWindow::showHotkeySettingsDialog);
}

void MainWindow::showSmartLanguageSettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Idiomas inteligentes do OCR"));

    auto *layout = new QVBoxLayout(&dialog);

    auto *formLayout = new QFormLayout();
    auto *primaryCombo = new QComboBox(&dialog);
    auto *secondaryCombo = new QComboBox(&dialog);

    for (const LanguageOption &language : languageOptions) {
        primaryCombo->addItem(tr(language.label), language.translateCode);
        primaryCombo->setItemData(primaryCombo->count() - 1,
                                  language.translateCode,
                                  Qt::UserRole + 1);
        secondaryCombo->addItem(tr(language.label), language.translateCode);
        secondaryCombo->setItemData(secondaryCombo->count() - 1,
                                    language.translateCode,
                                    Qt::UserRole + 1);
    }

    const int primaryIndex = languageIndexForTranslateCode(primaryCombo, m_primaryLanguage);
    if (primaryIndex >= 0) {
        primaryCombo->setCurrentIndex(primaryIndex);
    }

    const int secondaryIndex = languageIndexForTranslateCode(secondaryCombo, m_secondaryLanguage);
    if (secondaryIndex >= 0) {
        secondaryCombo->setCurrentIndex(secondaryIndex);
    }

    formLayout->addRow(tr("Idioma principal"), primaryCombo);
    formLayout->addRow(tr("Idioma secundário"), secondaryCombo);
    layout->addLayout(formLayout);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Salvar"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString primaryLanguage = primaryCombo->currentData(Qt::UserRole + 1).toString();
    const QString secondaryLanguage = secondaryCombo->currentData(Qt::UserRole + 1).toString();
    if (primaryLanguage == secondaryLanguage) {
        QMessageBox::warning(this,
                             tr("Idiomas inteligentes do OCR"),
                             tr("Escolha idiomas diferentes para o modo automático inteligente."));
        return;
    }

    m_primaryLanguage = primaryLanguage;
    m_secondaryLanguage = secondaryLanguage;
    saveSmartLanguageSettings();
    resetAutomaticTranslationMode();
    appendError(tr("Idiomas inteligentes do OCR atualizados."));
}

void MainWindow::showHotkeySettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Atalho OCR"));

    auto *layout = new QVBoxLayout(&dialog);

    auto *label = new QLabel(tr("Defina o atalho global para executar OCR."), &dialog);
    label->setWordWrap(true);
    layout->addWidget(label);

    auto *shortcutEdit = new QKeySequenceEdit(&dialog);
    shortcutEdit->setMaximumSequenceLength(1);

    const QKeySequence currentShortcut = QKeySequence::fromString(ocrHotkeyText(),
                                                                  QKeySequence::PortableText);
    shortcutEdit->setKeySequence(currentShortcut.isEmpty() ? defaultOcrHotkey() : currentShortcut);
    layout->addWidget(shortcutEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Salvar"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QKeySequence newShortcut = shortcutEdit->keySequence();
    if (newShortcut.isEmpty()) {
        QMessageBox::warning(this, tr("Atalho OCR"), tr("Escolha um atalho válido."));
        return;
    }

    const QKeySequence previousShortcut = currentShortcut.isEmpty() ? defaultOcrHotkey() : currentShortcut;
    if (registerOcrHotkey(newShortcut, true)) {
        appendError(tr("Atalho global OCR atualizado: %1").arg(newShortcut.toString(QKeySequence::NativeText)));
        return;
    }

    registerOcrHotkey(previousShortcut, false);
    const QString details = m_ocrHotkey ? m_ocrHotkey->lastError() : QString();
    const QString message = m_ocrHotkey && m_ocrHotkey->hasConflict()
                                ? tr("Esse atalho já está sendo usado por outro aplicativo. Escolha outro atalho.")
                                : tr("Não foi possível registrar esse atalho global.");
    QMessageBox::warning(this,
                         tr("Atalho OCR"),
                         details.isEmpty() ? message : message + QStringLiteral("\n\n") + details);
}

void MainWindow::showHelpWindow()
{
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Ajuda - AtlasHub"));

    auto *layout = new QVBoxLayout(dialog);

    auto *label = new QLabel(dialog);
    label->setWordWrap(true);
    label->setOpenExternalLinks(true);
    label->setTextFormat(Qt::RichText);
    label->setText(
        tr("Para suporte do AtlasHub, procure contato comigo pelo repositório:<br>"
        "<a href=\"https://github.com/Yurigo1793/Atlas-hub\">"
        "https://github.com/Yurigo1793/Atlas-hub</a>"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Fechar"));
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);

    layout->addWidget(label);
    layout->addWidget(buttons);

    dialog->resize(430, 150);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::showTrayCloseMessage()
{
    QSettings settings;
    if (settings.value(QStringLiteral("close/trayMessageShown"), false).toBool()) {
        return;
    }

    QMessageBox messageBox(this);
    messageBox.setIcon(QMessageBox::Information);
    messageBox.setWindowTitle(QStringLiteral("AtlasHub"));
    messageBox.setText(tr("AtlasHub continuará rodando na bandeja do sistema."));
    messageBox.setStandardButtons(QMessageBox::Ok);

    auto *doNotShowAgain = new QCheckBox(tr("Não mostrar novamente"), &messageBox);
    messageBox.setCheckBox(doNotShowAgain);
    messageBox.exec();

    settings.setValue(QStringLiteral("close/trayMessageShown"), true);
    settings.setValue(QStringLiteral("close/doNotShowTrayMessage"), doNotShowAgain->isChecked());
}

void MainWindow::startOcrCapture(bool restoreWindowWhenFinished)
{
    if (m_ocrInProgress) {
        appendError(tr("OCR já está em execução."));
        return;
    }

    m_ocrInProgress = true;
    setTechnicalStatus(QStringLiteral("ARMED"), QStringLiteral("IDLE"));
    ui->errorLog->setPlainText(tr("Selecione a área da tela para capturar..."));
    ui->textOutput->clear();
    ui->translationOutput->clear();

    if (isVisible()) {
        showMinimized();
    }

    QTimer::singleShot(300, this, [this, restoreWindowWhenFinished]() {
        auto *overlay = new ScreenCaptureOverlay();
        connect(overlay,
                &ScreenCaptureOverlay::captureFinished,
                this,
                [this, restoreWindowWhenFinished](const QString &path) {
                    handleOcrFinished(path, restoreWindowWhenFinished);
                });

        overlay->showFullScreen();
    });
}

void MainWindow::toggleSelectedHistoryFavorite()
{
    if (!m_historyList) {
        return;
    }

    QListWidgetItem *item = m_historyList->currentItem();
    if (!item) {
        return;
    }

    const int index = item->data(Qt::UserRole).toInt();
    if (index < 0 || index >= m_history.size()) {
        return;
    }

    m_history[index].favorite = !m_history.at(index).favorite;
    saveHistory();
    refreshHistoryView();
    appendError(m_history.at(index).favorite
                    ? tr("Captura adicionada aos favoritos.")
                    : tr("Captura removida dos favoritos."));
}

void MainWindow::quitFromTray()
{
    m_quitRequested = true;
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
    qApp->quit();
}

void MainWindow::syncUiLanguageActions()
{
    if (!m_uiLanguageGroup) {
        return;
    }

    for (QAction *action : m_uiLanguageGroup->actions()) {
        action->setChecked(action->data().toString() == m_uiLanguage);
    }
}
