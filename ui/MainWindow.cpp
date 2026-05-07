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
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFontComboBox>
#include <QIcon>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTimer>
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
    , m_ocrHotkey(nullptr)
    , m_quitRequested(false)
    , m_ocrInProgress(false)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/app.ico"));
    ui->appIconLabel->setPixmap(QIcon(":/icons/app.ico").pixmap(64, 64));

    setupTrayIcon();
    setupTextToolbar();
    setupHelp();
    setupLanguages();
    setupUiLanguageMenu();
    loadUiLanguageSettings();

    ui->errorLog->setPlainText(tr("Pronto para capturar OCR."));
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

    syncUiLanguageActions();
}

void MainWindow::setupHelp()
{
    ui->actionHelp->setText(tr("Ajuda"));
    ui->actionHelp->setToolTip(tr("Abrir ajuda"));
    connect(ui->actionHelp, &QAction::triggered, this, &MainWindow::showHelpWindow);
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

void MainWindow::setupTextToolbar()
{
    m_formatMenu = new QMenu(tr("Formatação"), this);
    menuBar()->insertMenu(ui->menuAjuda->menuAction(), m_formatMenu);

    m_formatToolbar = addToolBar(tr("Formatação"));
    m_formatToolbar->setMovable(false);

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
