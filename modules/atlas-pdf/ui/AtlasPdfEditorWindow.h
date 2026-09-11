#pragma once

#include "AtlasPdfDocument.h"
#include "AtlasPdfSelectableView.h"
#include "AtlasPdfService.h"
#include "AtlasPdfTextEngine.h"

#include <QHash>
#include <QImage>
#include <QMainWindow>
#include <QPdfDocument>
#include <QQueue>
#include <QSet>
#include <QSize>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui {
class AtlasPdfEditor;
}
QT_END_NAMESPACE

class QListWidgetItem;

class AtlasPdfEditorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AtlasPdfEditorWindow(QWidget *parent = nullptr);
    ~AtlasPdfEditorWindow() override;
    void openPdfFile(const QString &filePath);

private:
    struct OcrPageResult {
        int pageIndex = -1;
        int documentGeneration = 0;
        QString text;
        QVector<AtlasPdfSelectableView::TextBox> boxes;
        QString errorMessage;
    };

    void appendLog(const QString &message);
    int currentPageNumber() const;
    void loadPdf(const QString &filePath);
    void populatePages();
    QString applyOcrPageText(int pageIndex);
    void scheduleOcrPageText(int pageIndex);
    void applyOcrToCurrentPage();
    void applyOcrToDocument();
    void enqueueOcrPage(int pageIndex);
    void processNextQueuedOcrPage();
    void promptForImagePdfOcr();
    bool pageNeedsOcr(const AtlasPdfTextEngine::PageTextMap &pageMap, const QString &bestEffortText) const;
    QImage prepareOcrImage(const QImage &source) const;
    OcrPageResult extractOcrPageText(int pageIndex,
                                     int documentGeneration,
                                     const QString &imagePath,
                                     const QSize &visualImageSize,
                                     const QSize &ocrImageSize,
                                     const QSizeF &pageSize,
                                     int pageRotationDegrees) const;
    void refreshNativeText();
    void refreshDocumentState();
    void setupConnections();

    Ui::AtlasPdfEditor *ui;
    QPdfDocument m_pdfDocument;
    AtlasPdfDocument m_atlasDocument;
    AtlasPdfService m_pdfService;
    AtlasPdfTextEngine m_textEngine;
    AtlasPdfTextEngine::DocumentTextMap m_textMap;
    QHash<int, QString> m_ocrTextCache;
    QSet<int> m_ocrPagesInProgress;
    QQueue<int> m_pendingOcrPages;
    bool m_processingOcrQueue = false;
    bool m_ocrPromptActive = false;
    int m_ocrDocumentGeneration = 0;
};
