#pragma once

#include <QPdfSelection>
#include <QHash>
#include <QPdfView>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QVector>
#include <optional>

class AtlasPdfSelectableView : public QPdfView
{
    Q_OBJECT

public:
    struct TextBox {
        QString text;
        QRectF pageRect;
        int lineNumber = 0;
        int wordNumber = 0;
        qreal angleDegrees = 0.0;
        bool characterBox = false;
    };

    explicit AtlasPdfSelectableView(QWidget *parent = nullptr);

    QString selectedText() const;
    int selectedPageIndex() const;
    void setPdfFilePath(const QString &filePath);
    void setOcrTextBoxes(int pageIndex, const QVector<TextBox> &boxes);
    void clearOcrTextBoxes();
    void setSelectionCopyEnabled(bool enabled);
    void setSelectionCopyText(const QString &text);
    void clearTextSelection();

public slots:
    void selectAllCurrentPage();
    void copySelectionToClipboard();

signals:
    void textSelectionChanged(const QString &text);
    void zoomPercentChanged(int percent);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct PageViewport {
        int pageIndex = -1;
        QRect rect;
        qreal scale = 1.0;
        QSizeF pointSize;
    };

    QVector<PageViewport> pageViewports() const;
    bool pointToPagePoint(const QPoint &viewportPosition,
                          int *pageIndex,
                          QPointF *pagePoint) const;
    void updateCurrentSelection(const QPoint &viewportPosition);
    void updateSelectionText();
    QString selectedVisualText() const;
    QString selectedOcrText(const QRectF &pageRect) const;
    QRectF selectedPageRect() const;
    QRectF selectedVisualPageRect(const PageViewport &page) const;
    QRect selectedVisualViewportRect(const PageViewport &page) const;
    bool hasSelectedPageArea() const;
    bool copySelectedEmbeddedImageToClipboard();
    QVector<TextBox> selectedOcrBoxes(const QRectF &pageRect) const;
    QVector<QRectF> mergedOcrSelectionRects(const QVector<TextBox> &boxes,
                                            const QRectF &pageRect) const;
    QVector<QRectF> mergedTextSelectionRects(const QVector<QRectF> &rects) const;
    QVector<QRectF> visibleSelectionRects(const PageViewport &page) const;
    QVector<QRectF> selectedEmbeddedImageRects() const;
    QVector<QRectF> rasterSelectionRects(const PageViewport &page) const;

    bool m_selectionCopyEnabled = true;
    QString m_selectionCopyText;
    QString m_pdfFilePath;
    QHash<int, QVector<TextBox>> m_ocrTextBoxesByPage;
    bool m_draggingSelection = false;
    int m_selectionPage = -1;
    QPointF m_selectionStart;
    QPointF m_selectionEnd;
    QPoint m_selectionViewportStart;
    QPoint m_selectionViewportEnd;
    std::optional<QPdfSelection> m_selection;
};
