#include "AtlasPdfEditorWindow.h"
#include "ui_AtlasPdfEditor.h"

#include "AtlasPdfImageExtractor.h"
#include "AtlasPdfSelectableView.h"
#include "modules/atlas-ocr/ui/OCRService.h"

#include <QDateTime>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImage>
#include <QInputDialog>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMenu>
#include <QPdfPageNavigator>
#include <QPdfSelection>
#include <QPdfView>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringList>
#include <QStyle>
#include <QTabWidget>
#include <QTemporaryFile>
#include <QTextEdit>
#include <QTimer>
#include <QTransform>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>

namespace {

int normalizedRotationDegrees(int rotationDegrees)
{
    rotationDegrees %= 360;
    if (rotationDegrees < 0) {
        rotationDegrees += 360;
    }
    return rotationDegrees == 90 || rotationDegrees == 180 || rotationDegrees == 270 ? rotationDegrees : 0;
}

QImage orientedImageForOcr(const QImage &image, int pageRotationDegrees)
{
    const int rotation = normalizedRotationDegrees(pageRotationDegrees);
    if (rotation == 0 || image.isNull()) {
        return image;
    }

    QTransform transform;
    transform.rotate(rotation == 90 ? -90.0 : rotation == 270 ? 90.0 : 180.0);
    return image.transformed(transform, Qt::SmoothTransformation);
}

QRectF mapOcrRectToVisualImage(const QRectF &ocrRect, const QSize &visualImageSize, int pageRotationDegrees)
{
    const int rotation = normalizedRotationDegrees(pageRotationDegrees);
    if (rotation == 0 || visualImageSize.isEmpty()) {
        return ocrRect;
    }

    const qreal visualWidth = visualImageSize.width();
    const qreal visualHeight = visualImageSize.height();
    auto mapPoint = [rotation, visualWidth, visualHeight](const QPointF &point) {
        switch (rotation) {
        case 90:
            return QPointF(visualWidth - point.y(), point.x());
        case 180:
            return QPointF(visualWidth - point.x(), visualHeight - point.y());
        case 270:
            return QPointF(point.y(), visualHeight - point.x());
        default:
            return point;
        }
    };

    const QVector<QPointF> points = {
        mapPoint(ocrRect.topLeft()),
        mapPoint(ocrRect.topRight()),
        mapPoint(ocrRect.bottomLeft()),
        mapPoint(ocrRect.bottomRight()),
    };

    qreal minX = points.first().x();
    qreal maxX = minX;
    qreal minY = points.first().y();
    qreal maxY = minY;
    for (const QPointF &point : points) {
        minX = std::min(minX, point.x());
        maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y());
        maxY = std::max(maxY, point.y());
    }

    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
}

} // namespace

AtlasPdfEditorWindow::AtlasPdfEditorWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::AtlasPdfEditor)
    , m_pdfDocument(this)
{
    auto *central = new QWidget(this);
    ui->setupUi(central);
    setCentralWidget(central);
    setWindowTitle(tr("Atlas PDF"));
    resize(1220, 760);

    ui->btnOpenPdf->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    ui->btnSavePdf->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    ui->btnAddText->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    ui->btnHighlight->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    ui->btnSignature->setIcon(style()->standardIcon(QStyle::SP_DialogYesButton));
    ui->btnApplyOcr->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    ui->btnCopySelectedText->setIcon(style()->standardIcon(QStyle::SP_FileDialogContentsView));
    ui->btnCopyPageText->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));

    ui->btnOpenPdf->setToolTip(tr("Abrir PDF"));
    ui->btnSavePdf->setToolTip(tr("Salvar uma cópia com as edições preparadas"));
    ui->btnAddText->setToolTip(tr("Preparar inserção de texto"));
    ui->btnHighlight->setToolTip(tr("Preparar marca-texto"));
    ui->btnSignature->setToolTip(tr("Preparar assinatura"));
    ui->btnApplyOcr->setToolTip(tr("Aplicar OCR na página atual ou no documento"));
    ui->btnCopySelectedText->setToolTip(tr("Copiar a seleção atual"));
    ui->btnCopyPageText->setToolTip(tr("Copiar o texto da página atual"));
    ui->rightPanelTabs->setCurrentIndex(0);

    ui->pdfPageView->setDocument(&m_pdfDocument);
    ui->pdfPageView->setPageMode(QPdfView::PageMode::MultiPage);
    ui->pdfPageView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    ui->nativeTextEdit->setAcceptRichText(false);

    setupConnections();
    refreshDocumentState();
    appendLog(tr("Pronto para abrir um PDF."));
}

AtlasPdfEditorWindow::~AtlasPdfEditorWindow()
{
    delete ui;
}

void AtlasPdfEditorWindow::openPdfFile(const QString &filePath)
{
    if (!filePath.isEmpty()) {
        loadPdf(filePath);
    }
}

void AtlasPdfEditorWindow::appendLog(const QString &message)
{
    const QString line = QStringLiteral("[%1] %2")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                                  message);
    ui->pdfLog->appendPlainText(line);
}

int AtlasPdfEditorWindow::currentPageNumber() const
{
    return qMax(1, ui->pageSpinBox->value());
}

void AtlasPdfEditorWindow::loadPdf(const QString &filePath)
{
    m_atlasDocument = m_pdfService.openDocument(filePath);
    if (!m_atlasDocument.isValid()) {
        QMessageBox::warning(this, tr("Atlas PDF"), m_atlasDocument.errorMessage());
        appendLog(m_atlasDocument.errorMessage());
        return;
    }

    const QPdfDocument::Error error = m_pdfDocument.load(filePath);
    if (error != QPdfDocument::Error::None) {
        const QString message = tr("Não foi possível carregar o PDF para visualização.");
        QMessageBox::warning(this, tr("Atlas PDF"), message);
        appendLog(message);
        return;
    }

    populatePages();
    ui->pdfPageView->setPdfFilePath(filePath);
    ui->pdfPageView->clearOcrTextBoxes();
    m_ocrTextCache.clear();
    m_ocrPagesInProgress.clear();
    m_pendingOcrPages.clear();
    m_processingOcrQueue = false;
    ++m_ocrDocumentGeneration;
    m_textEngine.setPdfFilePath(filePath);
    ui->pdfPageView->clearTextSelection();
    m_textMap = AtlasPdfTextEngine::DocumentTextMap();
    refreshDocumentState();
    refreshNativeText();
    appendLog(tr("PDF aberto: %1").arg(m_atlasDocument.fileName()));
    appendLog(tr("Mapa de texto calculado sob demanda para manter a abertura rápida."));
    QTimer::singleShot(0, this, [this]() {
        promptForImagePdfOcr();
    });
}

void AtlasPdfEditorWindow::populatePages()
{
    ui->pageList->clear();
    for (int page = 0; page < m_pdfDocument.pageCount(); ++page) {
        auto *item = new QListWidgetItem(tr("Página %1").arg(page + 1), ui->pageList);
        item->setData(Qt::UserRole, page);
    }

    if (ui->pageList->count() > 0) {
        ui->pageList->setCurrentRow(0);
    }
}

void AtlasPdfEditorWindow::refreshDocumentState()
{
    const bool hasDocument = m_atlasDocument.isValid()
                             && m_pdfDocument.status() == QPdfDocument::Status::Ready;

    ui->btnSavePdf->setEnabled(hasDocument);
    ui->btnAddText->setEnabled(hasDocument);
    ui->btnHighlight->setEnabled(hasDocument);
    ui->btnSignature->setEnabled(hasDocument);
    ui->btnApplyOcr->setEnabled(hasDocument);
    ui->pageSpinBox->setEnabled(hasDocument);
    ui->zoomSlider->setEnabled(hasDocument);
    ui->editModeCombo->setEnabled(hasDocument);
    ui->btnCopySelectedText->setEnabled(hasDocument);
    ui->btnCopyPageText->setEnabled(hasDocument);

    ui->documentNameEdit->setText(hasDocument ? m_atlasDocument.fileName() : QString());
    ui->pageCountEdit->setText(hasDocument ? QString::number(m_pdfDocument.pageCount()) : QString());
    ui->pageSpinBox->setMaximum(qMax(1, m_pdfDocument.pageCount()));
    ui->textLayerStatusEdit->setText(hasDocument
                                         ? tr("%1 nativo / %2 Type3 / %3 quebrado")
                                               .arg(m_textMap.nativeTextPages)
                                               .arg(m_textMap.type3GlyphPages)
                                               .arg(m_textMap.brokenUnicodePages)
                                         : QString());

    if (!hasDocument) {
        ui->pdfPageView->clearTextSelection();
        ui->nativeTextEdit->clear();
    }
}

QString AtlasPdfEditorWindow::applyOcrPageText(int pageIndex)
{
    if (m_pdfDocument.status() != QPdfDocument::Status::Ready || pageIndex < 0
        || pageIndex >= m_pdfDocument.pageCount()) {
        return {};
    }

    const QSizeF pageSize = m_pdfDocument.pagePointSize(pageIndex);
    if (pageSize.isEmpty()) {
        return {};
    }

    const qreal renderScale = 3.0;
    const QSize imageSize(qMax(1, qRound(pageSize.width() * renderScale)),
                          qMax(1, qRound(pageSize.height() * renderScale)));
    const QImage pageImage = m_pdfDocument.render(pageIndex, imageSize);
    if (pageImage.isNull()) {
        return {};
    }

    QTemporaryFile imageFile(QDir::tempPath() + QStringLiteral("/atlas_pdf_ocr_XXXXXX.png"));
    imageFile.setAutoRemove(false);
    if (!imageFile.open()) {
        return {};
    }

    const QString imagePath = imageFile.fileName();
    imageFile.close();
    const int pageRotationDegrees = AtlasPdfImageExtractor(m_atlasDocument.filePath()).pageRotationDegrees(pageIndex);
    const QImage ocrImage = prepareOcrImage(orientedImageForOcr(pageImage, pageRotationDegrees));
    if (!ocrImage.save(imagePath, "PNG")) {
        QFile::remove(imagePath);
        return {};
    }

    const OcrPageResult result = extractOcrPageText(pageIndex,
                                                    m_ocrDocumentGeneration,
                                                    imagePath,
                                                    imageSize,
                                                    ocrImage.size(),
                                                    pageSize,
                                                    pageRotationDegrees);
    QFile::remove(imagePath);

    if (!result.errorMessage.isEmpty()) {
        appendLog(result.errorMessage);
        return {};
    }
    ui->pdfPageView->setOcrTextBoxes(pageIndex, result.boxes);
    return result.text;
}

void AtlasPdfEditorWindow::scheduleOcrPageText(int pageIndex)
{
    if (m_pdfDocument.status() != QPdfDocument::Status::Ready || pageIndex < 0
        || pageIndex >= m_pdfDocument.pageCount() || m_ocrPagesInProgress.contains(pageIndex)
        || m_ocrTextCache.contains(pageIndex)) {
        return;
    }

    const QSizeF pageSize = m_pdfDocument.pagePointSize(pageIndex);
    if (pageSize.isEmpty()) {
        return;
    }

    const qreal renderScale = 3.0;
    const QSize imageSize(qMax(1, qRound(pageSize.width() * renderScale)),
                          qMax(1, qRound(pageSize.height() * renderScale)));
    const QImage pageImage = m_pdfDocument.render(pageIndex, imageSize);
    if (pageImage.isNull()) {
        return;
    }

    QTemporaryFile imageFile(QDir::tempPath() + QStringLiteral("/atlas_pdf_ocr_XXXXXX.png"));
    imageFile.setAutoRemove(false);
    if (!imageFile.open()) {
        return;
    }

    const QString imagePath = imageFile.fileName();
    imageFile.close();

    const int pageRotationDegrees = AtlasPdfImageExtractor(m_atlasDocument.filePath()).pageRotationDegrees(pageIndex);
    const QImage ocrImage = prepareOcrImage(orientedImageForOcr(pageImage, pageRotationDegrees));
    if (!ocrImage.save(imagePath, "PNG")) {
        QFile::remove(imagePath);
        return;
    }

    m_ocrPagesInProgress.insert(pageIndex);
    const int documentGeneration = m_ocrDocumentGeneration;

    auto *watcher = new QFutureWatcher<OcrPageResult>(this);
    connect(watcher, &QFutureWatcher<OcrPageResult>::finished, this, [this, watcher, imagePath]() {
        const OcrPageResult result = watcher->result();
        watcher->deleteLater();
        QFile::remove(imagePath);
        if (result.documentGeneration != m_ocrDocumentGeneration) {
            return;
        }

        m_ocrPagesInProgress.remove(result.pageIndex);

        if (!result.errorMessage.isEmpty()) {
            appendLog(result.errorMessage);
            return;
        }

        if (!result.text.trimmed().isEmpty()) {
            m_ocrTextCache.insert(result.pageIndex, result.text);
            ui->pdfPageView->setOcrTextBoxes(result.pageIndex, result.boxes);
            appendLog(tr("OCR aplicado na página %1.").arg(result.pageIndex + 1));
        }

        if (result.pageIndex == currentPageNumber() - 1) {
            ui->nativeTextEdit->setPlainText(result.text);
            ui->textLayerStatusEdit->setText(result.text.trimmed().isEmpty() ? tr("Sem texto") : tr("OCR"));
        }
    });

    watcher->setFuture(QtConcurrent::run([this,
                                           pageIndex,
                                           documentGeneration,
                                           imagePath,
                                           imageSize,
                                           ocrImageSize = ocrImage.size(),
                                           pageSize,
                                           pageRotationDegrees]() {
        return extractOcrPageText(pageIndex,
                                  documentGeneration,
                                  imagePath,
                                  imageSize,
                                  ocrImageSize,
                                  pageSize,
                                  pageRotationDegrees);
    }));
}

void AtlasPdfEditorWindow::applyOcrToCurrentPage()
{
    if (m_pdfDocument.status() != QPdfDocument::Status::Ready) {
        return;
    }

    const int pageIndex = currentPageNumber() - 1;
    appendLog(tr("OCR solicitado na página %1.").arg(pageIndex + 1));
    m_ocrTextCache.remove(pageIndex);
    ui->pdfPageView->setOcrTextBoxes(pageIndex, {});
    scheduleOcrPageText(pageIndex);
    ui->textLayerStatusEdit->setText(tr("OCR em processamento"));
}

void AtlasPdfEditorWindow::applyOcrToDocument()
{
    if (m_pdfDocument.status() != QPdfDocument::Status::Ready) {
        return;
    }

    m_pendingOcrPages.clear();
    for (int pageIndex = 0; pageIndex < m_pdfDocument.pageCount(); ++pageIndex) {
        const AtlasPdfTextEngine::PageTextMap pageMap = m_textEngine.mapPage(&m_pdfDocument, pageIndex);
        const QString text = m_textEngine.bestEffortPageText(&m_pdfDocument, pageIndex);
        if (pageNeedsOcr(pageMap, text)) {
            enqueueOcrPage(pageIndex);
        }
    }

    appendLog(tr("OCR solicitado para páginas sem texto confiável: %1 página(s).").arg(m_pendingOcrPages.size()));
    processNextQueuedOcrPage();
}

void AtlasPdfEditorWindow::enqueueOcrPage(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= m_pdfDocument.pageCount()
        || m_ocrPagesInProgress.contains(pageIndex) || m_pendingOcrPages.contains(pageIndex)) {
        return;
    }

    m_pendingOcrPages.enqueue(pageIndex);
}

void AtlasPdfEditorWindow::processNextQueuedOcrPage()
{
    if (m_processingOcrQueue || m_pendingOcrPages.isEmpty()
        || m_pdfDocument.status() != QPdfDocument::Status::Ready) {
        return;
    }

    const int pageIndex = m_pendingOcrPages.dequeue();
    m_processingOcrQueue = true;

    const QSizeF pageSize = m_pdfDocument.pagePointSize(pageIndex);
    if (pageSize.isEmpty()) {
        m_processingOcrQueue = false;
        QTimer::singleShot(0, this, &AtlasPdfEditorWindow::processNextQueuedOcrPage);
        return;
    }

    const qreal renderScale = 3.0;
    const QSize imageSize(qMax(1, qRound(pageSize.width() * renderScale)),
                          qMax(1, qRound(pageSize.height() * renderScale)));
    const QImage pageImage = m_pdfDocument.render(pageIndex, imageSize);
    if (pageImage.isNull()) {
        m_processingOcrQueue = false;
        QTimer::singleShot(0, this, &AtlasPdfEditorWindow::processNextQueuedOcrPage);
        return;
    }

    QTemporaryFile imageFile(QDir::tempPath() + QStringLiteral("/atlas_pdf_ocr_XXXXXX.png"));
    imageFile.setAutoRemove(false);
    if (!imageFile.open()) {
        m_processingOcrQueue = false;
        QTimer::singleShot(0, this, &AtlasPdfEditorWindow::processNextQueuedOcrPage);
        return;
    }

    const QString imagePath = imageFile.fileName();
    imageFile.close();
    const int pageRotationDegrees = AtlasPdfImageExtractor(m_atlasDocument.filePath()).pageRotationDegrees(pageIndex);
    const QImage ocrImage = prepareOcrImage(orientedImageForOcr(pageImage, pageRotationDegrees));
    if (!ocrImage.save(imagePath, "PNG")) {
        QFile::remove(imagePath);
        m_processingOcrQueue = false;
        QTimer::singleShot(0, this, &AtlasPdfEditorWindow::processNextQueuedOcrPage);
        return;
    }

    m_ocrPagesInProgress.insert(pageIndex);
    const int documentGeneration = m_ocrDocumentGeneration;
    appendLog(tr("OCR do documento: processando página %1.").arg(pageIndex + 1));

    auto *watcher = new QFutureWatcher<OcrPageResult>(this);
    connect(watcher, &QFutureWatcher<OcrPageResult>::finished, this, [this, watcher, imagePath]() {
        const OcrPageResult result = watcher->result();
        watcher->deleteLater();
        QFile::remove(imagePath);

        if (result.documentGeneration == m_ocrDocumentGeneration) {
            m_ocrPagesInProgress.remove(result.pageIndex);
            if (!result.errorMessage.isEmpty()) {
                appendLog(result.errorMessage);
            } else if (!result.text.trimmed().isEmpty()) {
                m_ocrTextCache.insert(result.pageIndex, result.text);
                ui->pdfPageView->setOcrTextBoxes(result.pageIndex, result.boxes);
                appendLog(tr("OCR aplicado na página %1.").arg(result.pageIndex + 1));
                if (result.pageIndex == currentPageNumber() - 1) {
                    ui->nativeTextEdit->setPlainText(result.text);
                    ui->textLayerStatusEdit->setText(tr("OCR"));
                }
            }
        }

        m_processingOcrQueue = false;
        QTimer::singleShot(0, this, &AtlasPdfEditorWindow::processNextQueuedOcrPage);
    });

    watcher->setFuture(QtConcurrent::run([this,
                                           pageIndex,
                                           documentGeneration,
                                           imagePath,
                                           imageSize,
                                           ocrImageSize = ocrImage.size(),
                                           pageSize,
                                           pageRotationDegrees]() {
        return extractOcrPageText(pageIndex,
                                  documentGeneration,
                                  imagePath,
                                  imageSize,
                                  ocrImageSize,
                                  pageSize,
                                  pageRotationDegrees);
    }));
}

void AtlasPdfEditorWindow::promptForImagePdfOcr()
{
    if (m_pdfDocument.status() != QPdfDocument::Status::Ready || m_atlasDocument.filePath().isEmpty()) {
        return;
    }

    if (m_ocrPromptActive) {
        return;
    }

    const AtlasPdfImageExtractor extractor(m_atlasDocument.filePath());
    const AtlasPdfTextEngine::PageTextMap firstPageMap = m_textEngine.mapPage(&m_pdfDocument, 0);
    const QString firstPageText = m_textEngine.bestEffortPageText(&m_pdfDocument, 0);
    const bool firstPageNeedsOcr = pageNeedsOcr(firstPageMap, firstPageText);
    const bool hasImageEvidence = extractor.hasAnyPageImage(m_pdfDocument.pageCount())
                                  || firstPageMap.kind == AtlasPdfTextEngine::PageTextKind::NoText
                                  || firstPageMap.text.trimmed().isEmpty();
    if (!firstPageNeedsOcr || !hasImageEvidence) {
        return;
    }

    m_ocrPromptActive = true;
    QMessageBox prompt(this);
    prompt.setWindowTitle(tr("Atlas PDF"));
    prompt.setIcon(QMessageBox::Question);
    prompt.setText(tr("Este PDF contém imagem. Deseja aplicar OCR para converter em texto selecionável?"));
    QPushButton *yesButton = prompt.addButton(tr("Sim"), QMessageBox::YesRole);
    QPushButton *noButton = prompt.addButton(tr("Não"), QMessageBox::NoRole);
    prompt.setDefaultButton(noButton);
    prompt.exec();
    m_ocrPromptActive = false;
    if (prompt.clickedButton() == yesButton) {
        applyOcrToDocument();
    }
}

bool AtlasPdfEditorWindow::pageNeedsOcr(const AtlasPdfTextEngine::PageTextMap &pageMap,
                                        const QString &bestEffortText) const
{
    const QString trimmedText = bestEffortText.trimmed();
    if (!trimmedText.isEmpty() && !AtlasPdfTextEngine::looksCorruptText(trimmedText)) {
        return false;
    }

    switch (pageMap.kind) {
    case AtlasPdfTextEngine::PageTextKind::NativeText:
    case AtlasPdfTextEngine::PageTextKind::ActualText:
    case AtlasPdfTextEngine::PageTextKind::EncodedGlyphNames:
        return AtlasPdfTextEngine::looksCorruptText(pageMap.text) || pageMap.text.trimmed().isEmpty();
    case AtlasPdfTextEngine::PageTextKind::Type3GlyphShape:
    case AtlasPdfTextEngine::PageTextKind::BrokenUnicodeMap:
        return pageMap.text.trimmed().isEmpty();
    case AtlasPdfTextEngine::PageTextKind::NoText:
    case AtlasPdfTextEngine::PageTextKind::Error:
        return true;
    }

    return true;
}

QImage AtlasPdfEditorWindow::prepareOcrImage(const QImage &source) const
{
    if (source.isNull()) {
        return {};
    }

    const QImage input = source.convertToFormat(QImage::Format_ARGB32);
    QImage gray(input.size(), QImage::Format_Grayscale8);

    QVector<int> histogram(256, 0);
    histogram.fill(0);

    for (int y = 0; y < input.height(); ++y) {
        uchar *grayLine = gray.scanLine(y);
        const QRgb *inputLine = reinterpret_cast<const QRgb *>(input.constScanLine(y));
        for (int x = 0; x < input.width(); ++x) {
            const QColor color = QColor::fromRgba(inputLine[x]);
            const int alpha = color.alpha();
            const int luma = qBound(0,
                                    qRound(0.2126 * color.red() + 0.7152 * color.green()
                                           + 0.0722 * color.blue()),
                                    255);
            const int value = alpha < 20 ? 255 : luma;
            grayLine[x] = static_cast<uchar>(value);
            ++histogram[value];
        }
    }

    const int totalPixels = gray.width() * gray.height();
    auto percentile = [&histogram, totalPixels](double fraction) {
        const int target = qBound(0, qRound(totalPixels * fraction), totalPixels);
        int running = 0;
        for (int value = 0; value < histogram.size(); ++value) {
            running += histogram.at(value);
            if (running >= target) {
                return value;
            }
        }
        return 255;
    };

    const int low = percentile(0.03);
    const int high = percentile(0.97);
    const int range = qMax(1, high - low);

    QImage stretched(gray.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < gray.height(); ++y) {
        const uchar *grayLine = gray.constScanLine(y);
        uchar *stretchLine = stretched.scanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            const int value = qBound(0, (int(grayLine[x]) - low) * 255 / range, 255);
            stretchLine[x] = static_cast<uchar>(value);
        }
    }

    QImage output(stretched.size(), QImage::Format_Grayscale8);
    const int radius = qBound(10, qRound(std::min(stretched.width(), stretched.height()) * 0.012), 36);
    const int width = stretched.width();
    const int height = stretched.height();

    QVector<int> integral((width + 1) * (height + 1), 0);
    for (int y = 0; y < height; ++y) {
        const uchar *line = stretched.constScanLine(y);
        int rowSum = 0;
        for (int x = 0; x < width; ++x) {
            rowSum += line[x];
            integral[(y + 1) * (width + 1) + x + 1] = integral[y * (width + 1) + x + 1] + rowSum;
        }
    }

    for (int y = 0; y < height; ++y) {
        uchar *outLine = output.scanLine(y);
        const uchar *stretchLine = stretched.constScanLine(y);
        const int top = qMax(0, y - radius);
        const int bottom = qMin(height - 1, y + radius);
        for (int x = 0; x < width; ++x) {
            const int left = qMax(0, x - radius);
            const int right = qMin(width - 1, x + radius);
            const int area = (right - left + 1) * (bottom - top + 1);
            const int sum = integral[(bottom + 1) * (width + 1) + right + 1]
                            - integral[top * (width + 1) + right + 1]
                            - integral[(bottom + 1) * (width + 1) + left]
                            + integral[top * (width + 1) + left];
            const int localMean = sum / qMax(1, area);
            const int contrast = qBound(0, 128 + (int(stretchLine[x]) - localMean) * 4, 255);
            const int boosted = qBound(0, (contrast * 3 + int(stretchLine[x])) / 4, 255);
            outLine[x] = static_cast<uchar>(boosted);
        }
    }

    return output;
}

AtlasPdfEditorWindow::OcrPageResult AtlasPdfEditorWindow::extractOcrPageText(int pageIndex,
                                                                             int documentGeneration,
                                                                             const QString &imagePath,
                                                                             const QSize &visualImageSize,
                                                                             const QSize &ocrImageSize,
                                                                             const QSizeF &pageSize,
                                                                             int pageRotationDegrees) const
{
    OcrPageResult result;
    result.pageIndex = pageIndex;
    result.documentGeneration = documentGeneration;

    OCRService ocrService;
    const QVector<OCRService::OcrWord> words = ocrService.extractWords(imagePath, QStringLiteral("por+eng"));
    if (words.isEmpty()) {
        const QString text = ocrService.extractText(imagePath, QStringLiteral("por+eng")).trimmed();
        if (text.startsWith(QStringLiteral("Erro:"), Qt::CaseInsensitive)
            || text.startsWith(QStringLiteral("Falha:"), Qt::CaseInsensitive)) {
            result.errorMessage = text;
            return result;
        }
        result.text = text;
        return result;
    }

    QVector<AtlasPdfSelectableView::TextBox> boxes;
    const qreal scaleX = pageSize.width() / visualImageSize.width();
    const qreal scaleY = pageSize.height() / visualImageSize.height();
    boxes.reserve(words.size());
    for (const OCRService::OcrWord &word : words) {
        const QString wordText = word.text.trimmed();
        if (wordText.isEmpty()) {
            continue;
        }

        const QRectF wordRect(qreal(word.bounds.left()),
                              qreal(word.bounds.top()),
                              qreal(word.bounds.width()),
                              qreal(word.bounds.height()));
        const int lineNumber = word.blockNumber * 1000000 + word.paragraphNumber * 10000 + word.lineNumber;
        const qreal characterWidth = wordText.isEmpty() ? wordRect.width() : wordRect.width() / wordText.size();
        for (int characterIndex = 0; characterIndex < wordText.size(); ++characterIndex) {
            const QRectF characterOcrRect(wordRect.left() + characterIndex * characterWidth,
                                          wordRect.top(),
                                          characterWidth,
                                          wordRect.height());
            const QRectF characterVisualRect = mapOcrRectToVisualImage(characterOcrRect,
                                                                       visualImageSize,
                                                                       pageRotationDegrees);
            AtlasPdfSelectableView::TextBox box;
            box.text = wordText.mid(characterIndex, 1);
            box.pageRect = QRectF(characterVisualRect.left() * scaleX,
                                  characterVisualRect.top() * scaleY,
                                  characterVisualRect.width() * scaleX,
                                  characterVisualRect.height() * scaleY);
            box.lineNumber = lineNumber;
            box.wordNumber = word.wordNumber * 1000 + characterIndex;
            box.angleDegrees = word.angleDegrees;
            box.characterBox = true;
            boxes.push_back(box);
        }
    }

    QVector<OCRService::OcrWord> sortedWords = words;
    std::sort(sortedWords.begin(), sortedWords.end(), [](const OCRService::OcrWord &left,
                                                         const OCRService::OcrWord &right) {
        const int leftLine = left.blockNumber * 1000000 + left.paragraphNumber * 10000 + left.lineNumber;
        const int rightLine = right.blockNumber * 1000000 + right.paragraphNumber * 10000 + right.lineNumber;
        if (leftLine != rightLine) {
            return leftLine < rightLine;
        }
        return left.bounds.left() < right.bounds.left();
    });

    QStringList lines;
    int currentLine = sortedWords.first().blockNumber * 1000000
                      + sortedWords.first().paragraphNumber * 10000
                      + sortedWords.first().lineNumber;
    QString lineText;
    for (const OCRService::OcrWord &word : std::as_const(sortedWords)) {
        const int wordLine = word.blockNumber * 1000000 + word.paragraphNumber * 10000 + word.lineNumber;
        if (wordLine != currentLine) {
            if (!lineText.trimmed().isEmpty()) {
                lines.push_back(lineText.trimmed());
            }
            lineText.clear();
            currentLine = wordLine;
        }
        if (!lineText.isEmpty()) {
            lineText += QLatin1Char(' ');
        }
        lineText += word.text;
    }
    if (!lineText.trimmed().isEmpty()) {
        lines.push_back(lineText.trimmed());
    }

    result.boxes = boxes;
    result.text = lines.join(QLatin1Char('\n'));
    return result;
}

void AtlasPdfEditorWindow::refreshNativeText()
{
    if (m_pdfDocument.status() != QPdfDocument::Status::Ready) {
        ui->nativeTextEdit->clear();
        ui->textLayerStatusEdit->clear();
        return;
    }

    const int pageIndex = currentPageNumber() - 1;
    const AtlasPdfTextEngine::PageTextMap pageMap = m_textEngine.mapPage(&m_pdfDocument, pageIndex);
    ui->textLayerStatusEdit->setText(AtlasPdfTextEngine::kindLabel(pageMap.kind));
    QString text = m_textEngine.bestEffortPageText(&m_pdfDocument, pageIndex);
    const bool needsOcr = pageNeedsOcr(pageMap, text);
    if (needsOcr && m_ocrTextCache.contains(pageIndex)) {
        text = m_ocrTextCache.value(pageIndex);
        ui->textLayerStatusEdit->setText(tr("OCR"));
    } else if (needsOcr) {
        ui->textLayerStatusEdit->setText(m_ocrPagesInProgress.contains(pageIndex) ? tr("OCR em processamento")
                                                                                  : tr("OCR pendente"));
    }
    ui->nativeTextEdit->setPlainText(text);
}

void AtlasPdfEditorWindow::setupConnections()
{
    connect(ui->btnOpenPdf, &QPushButton::clicked, this, [this]() {
        const QString filePath = QFileDialog::getOpenFileName(this,
                                                              tr("Abrir PDF"),
                                                              QString(),
                                                              tr("PDF (*.pdf)"));
        if (!filePath.isEmpty()) {
            loadPdf(filePath);
        }
    });

    connect(ui->btnSavePdf, &QPushButton::clicked, this, [this]() {
        const QString suggestedName = QFileInfo(m_atlasDocument.filePath()).completeBaseName()
                                      + QStringLiteral("_atlas.pdf");
        const QString destinationPath = QFileDialog::getSaveFileName(this,
                                                                     tr("Salvar PDF"),
                                                                     suggestedName,
                                                                     tr("PDF (*.pdf)"));
        if (destinationPath.isEmpty()) {
            return;
        }

        QString errorMessage;
        if (!m_pdfService.saveEditedCopy(m_atlasDocument, destinationPath, &errorMessage)) {
            QMessageBox::warning(this, tr("Atlas PDF"), errorMessage);
            appendLog(errorMessage);
            return;
        }

        appendLog(tr("PDF salvo: %1").arg(destinationPath));
    });

    connect(ui->btnAddText, &QPushButton::clicked, this, [this]() {
        const QString text = QInputDialog::getText(this, tr("Texto"), tr("Texto para inserir"));
        if (text.trimmed().isEmpty()) {
            return;
        }

        m_atlasDocument.addEditOperation(m_pdfService.createTextOperation(
            currentPageNumber(), QRectF(72, 72, 320, 32), text.trimmed()));
        appendLog(tr("Texto preparado na página %1.").arg(currentPageNumber()));
    });

    connect(ui->btnHighlight, &QPushButton::clicked, this, [this]() {
        m_atlasDocument.addEditOperation(m_pdfService.createHighlightOperation(
            currentPageNumber(), QRectF(72, 120, 320, 36)));
        appendLog(tr("Marca-texto preparado na página %1.").arg(currentPageNumber()));
    });

    connect(ui->btnSignature, &QPushButton::clicked, this, [this]() {
        const QString signature = QInputDialog::getText(this,
                                                        tr("Assinatura"),
                                                        tr("Texto da assinatura"));
        if (signature.trimmed().isEmpty()) {
            return;
        }

        m_atlasDocument.addEditOperation(m_pdfService.createSignatureOperation(
            currentPageNumber(), QRectF(72, 180, 280, 48), signature.trimmed()));
        appendLog(tr("Assinatura preparada na página %1.").arg(currentPageNumber()));
    });

    auto *ocrMenu = new QMenu(ui->btnApplyOcr);
    QAction *ocrCurrentPageAction = ocrMenu->addAction(tr("Página atual"));
    QAction *ocrWholeDocumentAction = ocrMenu->addAction(tr("Documento todo"));
    ui->btnApplyOcr->setMenu(ocrMenu);
    connect(ocrCurrentPageAction, &QAction::triggered, this, &AtlasPdfEditorWindow::applyOcrToCurrentPage);
    connect(ocrWholeDocumentAction, &QAction::triggered, this, &AtlasPdfEditorWindow::applyOcrToDocument);

    connect(ui->pageSpinBox, &QSpinBox::valueChanged, this, [this](int value) {
        if (m_pdfDocument.status() == QPdfDocument::Status::Ready && ui->pdfPageView->pageNavigator()) {
            ui->pdfPageView->pageNavigator()->jump(value - 1, QPointF(), ui->pdfPageView->zoomFactor());
        }
        refreshNativeText();
    });

    auto jumpToPageItem = [this](QListWidgetItem *item) {
        if (!item) {
            return;
        }

        ui->pageSpinBox->setValue(item->data(Qt::UserRole).toInt() + 1);
    };

    connect(ui->pageList, &QListWidget::itemActivated, this, jumpToPageItem);
    connect(ui->pageList, &QListWidget::itemClicked, this, jumpToPageItem);

    connect(ui->zoomSlider, &QSlider::valueChanged, this, [this](int value) {
        ui->pdfPageView->setZoomMode(QPdfView::ZoomMode::Custom);
        ui->pdfPageView->setZoomFactor(value / 100.0);
    });

    connect(ui->pdfPageView, &AtlasPdfSelectableView::zoomPercentChanged, this, [this](int percent) {
        const QSignalBlocker blocker(ui->zoomSlider);
        ui->zoomSlider->setValue(qBound(ui->zoomSlider->minimum(), percent, ui->zoomSlider->maximum()));
    });

    connect(&m_pdfDocument, &QPdfDocument::statusChanged, this, [this]() {
        refreshDocumentState();
        refreshNativeText();
    });

    connect(ui->pdfPageView,
            &AtlasPdfSelectableView::textSelectionChanged,
            this,
            [this](const QString &text) {
        if (!text.trimmed().isEmpty()) {
            const int pageIndex = ui->pdfPageView->selectedPageIndex();
            const AtlasPdfTextEngine::PageTextMap pageMap = m_textEngine.mapPage(&m_pdfDocument, pageIndex);
            const QString decodedText = m_textEngine.decodedSelectionText(&m_pdfDocument, pageIndex, text);
            const bool decodedSelection = decodedText != text;

            if (!decodedText.isEmpty()
                && (pageMap.kind == AtlasPdfTextEngine::PageTextKind::BrokenUnicodeMap || decodedSelection
                    || AtlasPdfTextEngine::looksCorruptText(text))) {
                ui->pdfPageView->setSelectionCopyEnabled(true);
                ui->pdfPageView->setSelectionCopyText(decodedText);
                ui->nativeTextEdit->setPlainText(decodedText);
                ui->textLayerStatusEdit->setText(AtlasPdfTextEngine::kindLabel(pageMap.kind));
                QApplication::clipboard()->setText(decodedText);
                return;
            }

            if ((pageMap.kind == AtlasPdfTextEngine::PageTextKind::Type3GlyphShape
                 || pageMap.kind == AtlasPdfTextEngine::PageTextKind::BrokenUnicodeMap
                 || pageMap.kind == AtlasPdfTextEngine::PageTextKind::NoText
                 || AtlasPdfTextEngine::looksCorruptText(text))
                && AtlasPdfTextEngine::looksCorruptText(text)) {
                ui->pdfPageView->setSelectionCopyEnabled(false);
                ui->nativeTextEdit->clear();
                ui->textLayerStatusEdit->setText(AtlasPdfTextEngine::kindLabel(pageMap.kind));
                return;
            }

            ui->pdfPageView->setSelectionCopyEnabled(true);
            ui->pdfPageView->setSelectionCopyText(text);
            ui->nativeTextEdit->setPlainText(text);
            ui->textLayerStatusEdit->setText(tr("Seleção"));
        }
    });

    connect(ui->btnCopySelectedText, &QPushButton::clicked, this, [this]() {
        if (ui->pdfPageView->selectedPageIndex() >= 0) {
            ui->pdfPageView->copySelectionToClipboard();
            appendLog(tr("Texto selecionado no PDF copiado."));
            return;
        }

        const QString selectedText = ui->nativeTextEdit->textCursor().selectedText();
        const QString textToCopy = selectedText.isEmpty() ? ui->nativeTextEdit->toPlainText() : selectedText;
        QApplication::clipboard()->setText(textToCopy);
        appendLog(selectedText.isEmpty() ? tr("Texto da página copiado.")
                                         : tr("Texto selecionado copiado."));
    });

    connect(ui->btnCopyPageText, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(ui->nativeTextEdit->toPlainText());
        appendLog(tr("Texto da página copiado."));
    });

    auto hasDocument = [this]() {
        return m_atlasDocument.isValid() && m_pdfDocument.status() == QPdfDocument::Status::Ready;
    };

    auto addShortcut = [this](const QString &key, const auto &handler) {
        auto *shortcut = new QShortcut(QKeySequence(key), this);
        shortcut->setContext(Qt::ApplicationShortcut);
        connect(shortcut, &QShortcut::activated, this, handler);
    };

    auto jumpToPage = [this, hasDocument](int pageIndex) {
        if (!hasDocument()) {
            return;
        }
        ui->pageSpinBox->setValue(qBound(1, pageIndex + 1, m_pdfDocument.pageCount()));
    };

    auto movePage = [this, hasDocument, jumpToPage](int delta) {
        if (!hasDocument()) {
            return;
        }
        jumpToPage(ui->pageSpinBox->value() - 1 + delta);
    };

    auto scrollBy = [this](int dx, int dy) {
        ui->pdfPageView->horizontalScrollBar()->setValue(ui->pdfPageView->horizontalScrollBar()->value() + dx);
        ui->pdfPageView->verticalScrollBar()->setValue(ui->pdfPageView->verticalScrollBar()->value() + dy);
    };

    auto setZoom = [this, hasDocument](int value) {
        if (!hasDocument()) {
            return;
        }
        ui->zoomSlider->setValue(qBound(ui->zoomSlider->minimum(), value, ui->zoomSlider->maximum()));
    };

    auto zoomBy = [this, hasDocument, setZoom](int delta) {
        if (!hasDocument()) {
            return;
        }
        setZoom(ui->zoomSlider->value() + delta);
    };

    addShortcut(QStringLiteral("Ctrl+O"), [this]() { ui->btnOpenPdf->click(); });
    addShortcut(QStringLiteral("Ctrl+S"), [this, hasDocument]() { if (hasDocument()) ui->btnSavePdf->click(); });
    addShortcut(QStringLiteral("Ctrl+Shift+S"), [this, hasDocument]() { if (hasDocument()) ui->btnSavePdf->click(); });
    addShortcut(QStringLiteral("Ctrl+C"), [this, hasDocument]() { if (hasDocument()) ui->btnCopySelectedText->click(); });
    addShortcut(QStringLiteral("Ctrl+A"), [this, hasDocument]() { if (hasDocument()) ui->pdfPageView->selectAllCurrentPage(); });
    addShortcut(QStringLiteral("Escape"), [this]() { ui->pdfPageView->clearTextSelection(); });

    addShortcut(QStringLiteral("PageDown"), [movePage]() { movePage(1); });
    addShortcut(QStringLiteral("PageUp"), [movePage]() { movePage(-1); });
    addShortcut(QStringLiteral("Space"), [scrollBy]() { scrollBy(0, 520); });
    addShortcut(QStringLiteral("Shift+Space"), [scrollBy]() { scrollBy(0, -520); });
    addShortcut(QStringLiteral("Home"), [scrollBy]() { scrollBy(0, -240); });
    addShortcut(QStringLiteral("End"), [scrollBy]() { scrollBy(0, 240); });
    addShortcut(QStringLiteral("Ctrl+Home"), [jumpToPage]() { jumpToPage(0); });
    addShortcut(QStringLiteral("Ctrl+End"), [this, hasDocument, jumpToPage]() { if (hasDocument()) jumpToPage(m_pdfDocument.pageCount() - 1); });
    addShortcut(QStringLiteral("Alt+Left"), [movePage]() { movePage(-1); });
    addShortcut(QStringLiteral("Alt+Right"), [movePage]() { movePage(1); });
    addShortcut(QStringLiteral("Up"), [scrollBy]() { scrollBy(0, -48); });
    addShortcut(QStringLiteral("Down"), [scrollBy]() { scrollBy(0, 48); });
    addShortcut(QStringLiteral("Left"), [scrollBy]() { scrollBy(-48, 0); });
    addShortcut(QStringLiteral("Right"), [scrollBy]() { scrollBy(48, 0); });

    addShortcut(QStringLiteral("Ctrl++"), [zoomBy]() { zoomBy(10); });
    addShortcut(QStringLiteral("Ctrl+="), [zoomBy]() { zoomBy(10); });
    addShortcut(QStringLiteral("Ctrl+-"), [zoomBy]() { zoomBy(-10); });
    addShortcut(QStringLiteral("Ctrl+0"), [this, hasDocument]() {
        if (!hasDocument()) {
            return;
        }
        ui->pdfPageView->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    });
    addShortcut(QStringLiteral("Ctrl+1"), [setZoom]() { setZoom(100); });
    addShortcut(QStringLiteral("Ctrl+2"), [this, hasDocument]() {
        if (!hasDocument()) {
            return;
        }
        ui->pdfPageView->setZoomMode(QPdfView::ZoomMode::FitInView);
    });
    addShortcut(QStringLiteral("Ctrl+3"), [setZoom]() { setZoom(150); });

    addShortcut(QStringLiteral("Alt+1"), [this, hasDocument]() { if (hasDocument()) ui->editModeCombo->setCurrentIndex(0); });
    addShortcut(QStringLiteral("Alt+2"), [this, hasDocument]() { if (hasDocument()) ui->editModeCombo->setCurrentIndex(1); });
    addShortcut(QStringLiteral("Alt+3"), [this, hasDocument]() { if (hasDocument()) ui->editModeCombo->setCurrentIndex(2); });
    addShortcut(QStringLiteral("Alt+4"), [this, hasDocument]() { if (hasDocument()) ui->editModeCombo->setCurrentIndex(3); });
    addShortcut(QStringLiteral("Alt+T"), [this, hasDocument]() { if (hasDocument()) ui->btnAddText->click(); });
    addShortcut(QStringLiteral("Alt+H"), [this, hasDocument]() { if (hasDocument()) ui->btnHighlight->click(); });
    addShortcut(QStringLiteral("Alt+S"), [this, hasDocument]() { if (hasDocument()) ui->btnSignature->click(); });

    addShortcut(QStringLiteral("Ctrl+R"), [this, hasDocument]() { if (hasDocument()) applyOcrToCurrentPage(); });
    addShortcut(QStringLiteral("Ctrl+Shift+R"), [this, hasDocument]() { if (hasDocument()) applyOcrToDocument(); });
    addShortcut(QStringLiteral("Ctrl+Shift+C"), [this, hasDocument]() { if (hasDocument()) ui->btnCopyPageText->click(); });

    addShortcut(QStringLiteral("F5"), [this]() {
        ui->pageSpinBox->setFocus(Qt::ShortcutFocusReason);
        ui->pageSpinBox->selectAll();
    });
    addShortcut(QStringLiteral("Ctrl+G"), [this]() {
        ui->pageSpinBox->setFocus(Qt::ShortcutFocusReason);
        ui->pageSpinBox->selectAll();
    });
    addShortcut(QStringLiteral("Ctrl+L"), [this]() {
        ui->pageSpinBox->setFocus(Qt::ShortcutFocusReason);
        ui->pageSpinBox->selectAll();
    });
    addShortcut(QStringLiteral("F6"), [this]() { ui->pdfPageView->setFocus(Qt::ShortcutFocusReason); });
    addShortcut(QStringLiteral("Ctrl+F"), [this]() {
        ui->rightPanelTabs->setCurrentWidget(ui->textPanel);
        ui->nativeTextEdit->setFocus(Qt::ShortcutFocusReason);
    });
    addShortcut(QStringLiteral("Ctrl+Tab"), [this]() {
        ui->rightPanelTabs->setCurrentIndex((ui->rightPanelTabs->currentIndex() + 1) % ui->rightPanelTabs->count());
    });
    addShortcut(QStringLiteral("Ctrl+Shift+Tab"), [this]() {
        const int count = ui->rightPanelTabs->count();
        ui->rightPanelTabs->setCurrentIndex((ui->rightPanelTabs->currentIndex() + count - 1) % count);
    });
}
