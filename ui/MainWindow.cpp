#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "core/OCRService.h"
#include "core/TranslationService.h"
#include "ui/ScreenCaptureOverlay.h"
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
#include <QIcon>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
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
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_uiLanguage(QStringLiteral("pt_BR"))
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
    , m_copyOcrButton(nullptr)
    , m_copyTranslationButton(nullptr)
    , m_historyDock(nullptr)
    , m_historyList(nullptr)
    , m_ocrHotkey(nullptr)
    , m_quitRequested(false)
    , m_ocrInProgress(false)
    , m_darkTheme(false)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/app.ico"));
    ui->appIconLabel->setPixmap(QIcon(":/icons/app.ico").pixmap(40, 40));
    ui->btnRunOCR->setStyleSheet(QString());
    ui->btnRunOCR->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    ui->btnTranslate->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    ui->errorLog->setMaximumHeight(58);
    ui->errorLog->setMinimumHeight(44);
    ui->mainLayout->setContentsMargins(10, 10, 10, 8);
    ui->mainLayout->setSpacing(8);
    ui->contentLayout->setSpacing(10);
    ui->controlsLayout->setSpacing(6);
    statusBar()->setMaximumHeight(20);
    statusBar()->setSizeGripEnabled(false);

    setupTrayIcon();
    setupCopyButtons();
    setupTextToolbar();
    setupHelp();
    setupLanguages();
    setupUiLanguageMenu();
    setupSettingsActions();
    setupHistory();
    loadUiLanguageSettings();
    loadHistory();

    ui->errorLog->setPlainText(tr("Pronto para capturar OCR."));
    statusBar()->showMessage(tr("Pronto."));
    applyTheme();
    setupHotkey();

    connect(ui->btnRunOCR, &QPushButton::clicked, this, [this]() {
        startOcrCapture(true);
    });

    connect(ui->btnTranslate, &QPushButton::clicked, this, &MainWindow::runTranslation);
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

    if (!m_darkTheme) {
        qApp->setStyleSheet(QStringLiteral(
            "QMainWindow, QWidget { background: #f6f7f9; color: #20242a; }"
            "QTextEdit, QPlainTextEdit, QComboBox, QListWidget { background: #ffffff; border: 1px solid #d7dce2; border-radius: 6px; padding: 5px; selection-background-color: #2f80ed; }"
            "QPushButton, QToolButton { background: #ffffff; border: 1px solid #ccd3db; border-radius: 6px; padding: 5px 8px; }"
            "QPushButton:hover, QToolButton:hover { background: #eef4fb; border-color: #9eb6d4; }"
            "QPushButton:pressed, QToolButton:pressed { background: #dceafb; }"
            "QPushButton#btnRunOCR { background: #1976d2; color: white; border-color: #155fa8; font-weight: 600; }"
            "QPushButton#btnTranslate { background: #22313f; color: white; border-color: #1a2631; font-weight: 600; }"
            "QLabel { background: transparent; }"
            "QMenuBar, QMenu, QStatusBar, QToolBar { background: #eef1f4; color: #20242a; }"
            "QDockWidget::title { padding: 4px; background: #e4e8ed; }"));
        return;
    }

    qApp->setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget { background: #20252b; color: #edf1f5; }"
        "QTextEdit, QPlainTextEdit, QComboBox, QListWidget { background: #15191e; color: #edf1f5; border: 1px solid #3b4652; border-radius: 6px; padding: 5px; selection-background-color: #3d7fc9; }"
        "QPushButton, QToolButton { background: #2a3139; color: #edf1f5; border: 1px solid #465260; border-radius: 6px; padding: 5px 8px; }"
        "QPushButton:hover, QToolButton:hover { background: #35404a; border-color: #5d6d7d; }"
        "QPushButton:pressed, QToolButton:pressed { background: #1c2228; }"
        "QPushButton#btnRunOCR { background: #2f80ed; color: white; border-color: #2567bf; font-weight: 600; }"
        "QPushButton#btnTranslate { background: #3c4956; color: white; border-color: #536273; font-weight: 600; }"
        "QLabel { background: transparent; }"
        "QMenuBar, QMenu, QStatusBar, QToolBar { background: #181d22; color: #edf1f5; }"
        "QDockWidget::title { padding: 4px; background: #252c33; }"));
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
    m_history.prepend(entry);

    while (m_history.size() > 30) {
        m_history.removeLast();
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
        m_ocrInProgress = false;
        if (restoreWindowWhenFinished) {
            restoreFromTray();
        }
        return;
    }

    appendError(tr("Imagem capturada. Executando OCR..."));
    QCoreApplication::processEvents();

    OCRService ocrService;
    const QString result = ocrService.extractText(path, ui->sourceLanguageCombo->currentData().toString());
    const QString visibleResult = result.trimmed();

    if (visibleResult.isEmpty()) {
        appendError(tr("OCR concluído, mas nenhum texto foi reconhecido."));
        m_ocrInProgress = false;
        if (restoreWindowWhenFinished) {
            restoreFromTray();
        }
        return;
    }

    if (isErrorMessage(visibleResult)) {
        appendError(visibleResult);
        m_ocrInProgress = false;
        if (restoreWindowWhenFinished) {
            restoreFromTray();
        }
        return;
    }

    appendError(tr("OCR concluído."));
    ui->textOutput->setPlainText(result);
    runTranslation();
    addHistoryEntry(result, ui->translationOutput->toPlainText());

    m_ocrInProgress = false;
    if (restoreWindowWhenFinished) {
        restoreFromTray();
    }
    ui->textOutput->setFocus();
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

void MainWindow::runTranslation()
{
    const QString text = ui->textOutput->toPlainText();
    if (text.trimmed().isEmpty()) {
        appendError(tr("Falha: não há texto de OCR para traduzir."));
        return;
    }

    ui->btnTranslate->setEnabled(false);
    ui->translationOutput->setPlainText(tr("Traduzindo..."));
    QCoreApplication::processEvents();

    TranslationService translationService;
    const QString detectedLanguage = translationService.detectLanguage(text);
    const int detectedLanguageIndex = sourceLanguageIndexForTranslateCode(detectedLanguage);
    if (detectedLanguageIndex >= 0 && detectedLanguageIndex != ui->sourceLanguageCombo->currentIndex()) {
        ui->sourceLanguageCombo->setCurrentIndex(detectedLanguageIndex);
        appendError(tr("Idioma detectado automaticamente: %1.").arg(ui->sourceLanguageCombo->currentText()));
        QCoreApplication::processEvents();
    }

    const QString result = translationService.translateText(
        text,
        ui->sourceLanguageCombo->currentData(Qt::UserRole + 1).toString(),
        ui->targetLanguageCombo->currentData(Qt::UserRole + 1).toString());

    ui->btnTranslate->setEnabled(true);

    if (isErrorMessage(result.trimmed())) {
        ui->translationOutput->clear();
        appendError(result.trimmed());
        return;
    }

    ui->translationOutput->setPlainText(result);
    appendError(tr("Tradução concluída."));
    addHistoryEntry(text, result);
}

int MainWindow::sourceLanguageIndexForTranslateCode(const QString &languageCode) const
{
    for (int i = 0; i < ui->sourceLanguageCombo->count(); ++i) {
        if (ui->sourceLanguageCombo->itemData(i, Qt::UserRole + 1).toString() == languageCode) {
            return i;
        }
    }

    return -1;
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

    for (int i = 0; i < m_history.size(); ++i) {
        const HistoryEntry &entry = m_history.at(i);
        const QDateTime timestamp = QDateTime::fromString(entry.timestamp, Qt::ISODate);
        QString title = timestamp.isValid()
                            ? timestamp.toString(QStringLiteral("dd/MM HH:mm"))
                            : tr("Captura");

        const QString previewSource = entry.translationText.isEmpty() ? entry.ocrText : entry.translationText;
        QString preview = previewSource.simplified();
        if (preview.size() > 76) {
            preview = preview.left(73) + QStringLiteral("...");
        }

        auto *item = new QListWidgetItem(QStringLiteral("%1  %2").arg(title, preview), m_historyList);
        item->setData(Qt::UserRole, i);
        item->setToolTip(tr("Clique duas vezes para restaurar esta captura."));
    }
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
        array.append(object);
    }

    QSettings settings;
    settings.setValue(QStringLiteral("history/items"), QJsonDocument(array).toJson(QJsonDocument::Compact));
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

    syncUiLanguageActions();
}

void MainWindow::setupCopyButtons()
{
    m_copyOcrButton = new QToolButton(this);
    m_copyOcrButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    m_copyOcrButton->setToolTip(tr("Copiar texto OCR"));
    m_copyOcrButton->setAutoRaise(true);

    m_copyTranslationButton = new QToolButton(this);
    m_copyTranslationButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    m_copyTranslationButton->setToolTip(tr("Copiar tradução"));
    m_copyTranslationButton->setAutoRaise(true);

    auto *ocrHeaderLayout = new QHBoxLayout();
    ocrHeaderLayout->setContentsMargins(0, 0, 0, 0);
    ocrHeaderLayout->setSpacing(6);
    ui->ocrPanelLayout->removeWidget(ui->ocrTextLabel);
    ocrHeaderLayout->addWidget(ui->ocrTextLabel);
    ocrHeaderLayout->addStretch();
    ocrHeaderLayout->addWidget(m_copyOcrButton);
    ui->ocrPanelLayout->insertLayout(0, ocrHeaderLayout);

    auto *translationHeaderLayout = new QHBoxLayout();
    translationHeaderLayout->setContentsMargins(0, 0, 0, 0);
    translationHeaderLayout->setSpacing(6);
    ui->translationPanelLayout->removeWidget(ui->translationTextLabel);
    translationHeaderLayout->addWidget(ui->translationTextLabel);
    translationHeaderLayout->addStretch();
    translationHeaderLayout->addWidget(m_copyTranslationButton);
    ui->translationPanelLayout->insertLayout(0, translationHeaderLayout);

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
    m_historyDock = new QDockWidget(tr("Histórico"), this);
    m_historyDock->setObjectName(QStringLiteral("historyDock"));
    m_historyDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);

    m_historyList = new QListWidget(m_historyDock);
    m_historyList->setAlternatingRowColors(true);
    m_historyList->setUniformItemSizes(true);
    m_historyDock->setWidget(m_historyList);
    addDockWidget(Qt::BottomDockWidgetArea, m_historyDock);
    m_historyDock->setMaximumHeight(150);

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
        saveTranslationLanguageSettings();
    });

    connect(ui->targetLanguageCombo, &QComboBox::currentIndexChanged, this, [this]() {
        saveTranslationLanguageSettings();
    });
}

void MainWindow::setupTrayIcon()
{
    m_trayMenu = new QMenu(this);

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

    m_startWithWindowsAction = ui->menuconf->addAction(tr("Inicializar com Windows"));
    m_startWithWindowsAction->setCheckable(true);
    m_startWithWindowsAction->setChecked(isWindowsStartupEnabled());

    m_startMinimizedAction = ui->menuconf->addAction(tr("Iniciar minimizado na tray"));
    m_startMinimizedAction->setCheckable(true);
    m_startMinimizedAction->setChecked(settings.value(QStringLiteral("startup/startMinimizedToTray"), false).toBool());

    m_darkThemeAction = ui->menuconf->addAction(tr("Tema escuro"));
    m_darkThemeAction->setCheckable(true);
    m_darkThemeAction->setChecked(settings.value(QStringLiteral("ui/darkTheme"), false).toBool());

    m_clearHistoryAction = ui->menuconf->addAction(tr("Limpar histórico"));

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

    connect(m_clearHistoryAction, &QAction::triggered, this, [this]() {
        m_history.clear();
        saveHistory();
        refreshHistoryView();
        appendError(tr("Histórico limpo."));
    });
}

void MainWindow::setupTextToolbar()
{
    m_formatMenu = new QMenu(tr("Formatação"), this);
    menuBar()->insertMenu(ui->menuAjuda->menuAction(), m_formatMenu);

    m_formatToolbar = addToolBar(tr("Formatação"));
    m_formatToolbar->setMovable(false);
    m_formatToolbar->setIconSize(QSize(16, 16));

    m_fontBox = new QFontComboBox(m_formatToolbar);
    m_fontBox->setToolTip(tr("Fonte"));
    m_formatToolbar->addWidget(m_fontBox);

    m_sizeBox = new QSpinBox(m_formatToolbar);
    m_sizeBox->setToolTip(tr("Tamanho"));
    m_sizeBox->setRange(8, 48);
    m_sizeBox->setValue(12);
    m_sizeBox->setSuffix(tr(" pt"));
    m_formatToolbar->addWidget(m_sizeBox);

    m_formatToolbar->addSeparator();

    m_boldAction = new QAction(tr("Negrito"), this);
    m_boldAction->setIconText(tr("B"));
    m_boldAction->setCheckable(true);
    m_boldAction->setToolTip(tr("Negrito"));

    m_italicAction = new QAction(tr("Itálico"), this);
    m_italicAction->setIconText(tr("I"));
    m_italicAction->setCheckable(true);
    m_italicAction->setToolTip(tr("Itálico"));

    m_underlineAction = new QAction(tr("Sublinhado"), this);
    m_underlineAction->setIconText(tr("U"));
    m_underlineAction->setCheckable(true);
    m_underlineAction->setToolTip(tr("Sublinhado"));

    m_colorAction = new QAction(tr("Cor do texto"), this);
    m_colorAction->setIconText(tr("Cor"));
    m_colorAction->setToolTip(tr("Cor do texto"));

    m_formatToolbar->addAction(m_boldAction);
    m_formatToolbar->addAction(m_italicAction);
    m_formatToolbar->addAction(m_underlineAction);
    m_formatToolbar->addAction(m_colorAction);

    m_formatMenu->addAction(m_boldAction);
    m_formatMenu->addAction(m_italicAction);
    m_formatMenu->addAction(m_underlineAction);
    m_formatMenu->addSeparator();
    m_formatMenu->addAction(m_colorAction);

    connect(m_fontBox, &QFontComboBox::currentFontChanged, this, [this](const QFont &font) {
        QTextCharFormat format;
        format.setFontFamilies(QStringList{font.family()});
        mergeOutputFormat(format);
    });

    connect(m_sizeBox, &QSpinBox::valueChanged, this, [this](int size) {
        QTextCharFormat format;
        format.setFontPointSize(size);
        mergeOutputFormat(format);
    });

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
    m_uiLanguageMenu = ui->menuconf->addMenu(tr("Idioma da interface"));
    m_uiLanguageGroup = new QActionGroup(this);
    m_uiLanguageGroup->setExclusive(true);

    m_portugueseAction = m_uiLanguageMenu->addAction(tr("Português", "UI language option"));
    m_portugueseAction->setCheckable(true);
    m_portugueseAction->setData(QStringLiteral("pt_BR"));
    m_uiLanguageGroup->addAction(m_portugueseAction);

    m_englishAction = m_uiLanguageMenu->addAction(tr("English", "UI language option"));
    m_englishAction->setCheckable(true);
    m_englishAction->setData(QStringLiteral("en_US"));
    m_uiLanguageGroup->addAction(m_englishAction);

    m_frenchAction = m_uiLanguageMenu->addAction(tr("Français", "UI language option"));
    m_frenchAction->setCheckable(true);
    m_frenchAction->setData(QStringLiteral("fr_FR"));
    m_uiLanguageGroup->addAction(m_frenchAction);

    connect(m_uiLanguageGroup, &QActionGroup::triggered, this, [this](QAction *action) {
        applyUiLanguage(action->data().toString());
    });

    ui->menuconf->addSeparator();
    m_configureHotkeyAction = ui->menuconf->addAction(tr("Atalho OCR..."));
    connect(m_configureHotkeyAction, &QAction::triggered, this, &MainWindow::showHotkeySettingsDialog);
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
