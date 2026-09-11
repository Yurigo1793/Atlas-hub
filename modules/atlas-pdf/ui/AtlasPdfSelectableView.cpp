#include "AtlasPdfSelectableView.h"

#include "../engine/AtlasPdfImageExtractor.h"
#include "../engine/AtlasPdfTextEngine.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPdfDocument>
#include <QPdfPageNavigator>
#include <QScrollBar>
#include <QStringList>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <utility>

AtlasPdfSelectableView::AtlasPdfSelectableView(QWidget *parent)
    : QPdfView(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setFocusPolicy(Qt::StrongFocus);
    viewport()->installEventFilter(this);
    setCursor(Qt::IBeamCursor);
}

QString AtlasPdfSelectableView::selectedText() const
{
    if (!m_draggingSelection && !m_selectionCopyText.trimmed().isEmpty()) {
        return m_selectionCopyText;
    }

    if (m_selectionPage >= 0 && hasSelectedPageArea() && m_ocrTextBoxesByPage.contains(m_selectionPage)) {
        const QVector<PageViewport> viewports = pageViewports();
        const auto pageIt = std::find_if(viewports.cbegin(), viewports.cend(), [this](const PageViewport &page) {
            return page.pageIndex == m_selectionPage;
        });
        if (pageIt != viewports.cend()) {
            const QString ocrText = selectedOcrText(selectedVisualPageRect(*pageIt));
            if (!ocrText.trimmed().isEmpty()) {
                return ocrText;
            }
        }
    }

    const QString nativeText = m_selection && m_selection->isValid() ? m_selection->text() : QString();
    if (!nativeText.trimmed().isEmpty()) {
        return nativeText;
    }

    if (m_selectionPage >= 0 && hasSelectedPageArea()) {
        const QVector<PageViewport> viewports = pageViewports();
        const auto pageIt = std::find_if(viewports.cbegin(), viewports.cend(), [this](const PageViewport &page) {
            return page.pageIndex == m_selectionPage;
        });
        if (pageIt != viewports.cend()) {
            return selectedOcrText(selectedVisualPageRect(*pageIt));
        }
    }

    return {};
}

int AtlasPdfSelectableView::selectedPageIndex() const
{
    return m_selectionPage;
}

void AtlasPdfSelectableView::setPdfFilePath(const QString &filePath)
{
    m_pdfFilePath = filePath;
}

void AtlasPdfSelectableView::setOcrTextBoxes(int pageIndex, const QVector<TextBox> &boxes)
{
    if (pageIndex < 0) {
        return;
    }

    if (boxes.isEmpty()) {
        m_ocrTextBoxesByPage.remove(pageIndex);
    } else {
        m_ocrTextBoxesByPage.insert(pageIndex, boxes);
    }
}

void AtlasPdfSelectableView::clearOcrTextBoxes()
{
    m_ocrTextBoxesByPage.clear();
}

void AtlasPdfSelectableView::setSelectionCopyEnabled(bool enabled)
{
    m_selectionCopyEnabled = enabled;
}

void AtlasPdfSelectableView::setSelectionCopyText(const QString &text)
{
    m_selectionCopyText = text;
}

void AtlasPdfSelectableView::clearTextSelection()
{
    m_selectionCopyEnabled = true;
    m_selectionCopyText.clear();
    m_draggingSelection = false;
    m_selectionPage = -1;
    m_selectionStart = QPointF();
    m_selectionEnd = QPointF();
    m_selectionViewportStart = QPoint();
    m_selectionViewportEnd = QPoint();
    m_selection.reset();
    updateSelectionText();
    viewport()->update();
}

void AtlasPdfSelectableView::selectAllCurrentPage()
{
    QPdfDocument *pdfDocument = document();
    if (!pdfDocument || pdfDocument->status() != QPdfDocument::Status::Ready || pdfDocument->pageCount() <= 0) {
        return;
    }

    int pageIndex = pageNavigator() ? pageNavigator()->currentPage() : -1;
    const QVector<PageViewport> viewports = pageViewports();
    if (pageIndex < 0 || pageIndex >= pdfDocument->pageCount()) {
        for (const PageViewport &page : viewports) {
            if (page.rect.intersects(viewport()->rect())) {
                pageIndex = page.pageIndex;
                break;
            }
        }
    }
    if (pageIndex < 0 || pageIndex >= pdfDocument->pageCount()) {
        pageIndex = 0;
    }

    const QSizeF pageSize = pdfDocument->pagePointSize(pageIndex);
    if (pageSize.isEmpty()) {
        return;
    }

    m_draggingSelection = false;
    m_selectionPage = pageIndex;
    m_selectionStart = QPointF(0.0, 0.0);
    m_selectionEnd = QPointF(pageSize.width(), pageSize.height());
    m_selectionViewportStart = QPoint(0, 0);
    m_selectionViewportEnd = viewport()->rect().bottomRight();
    m_selectionCopyText.clear();
    m_selection = pdfDocument->getAllText(m_selectionPage);

    const auto pageIt = std::find_if(viewports.cbegin(), viewports.cend(), [pageIndex](const PageViewport &page) {
        return page.pageIndex == pageIndex;
    });
    if (pageIt != viewports.cend()) {
        m_selectionViewportStart = pageIt->rect.topLeft();
        m_selectionViewportEnd = pageIt->rect.bottomRight();
    }

    if (m_selection && m_selection->isValid() && !m_selection->text().trimmed().isEmpty()) {
        m_selectionCopyText = m_selection->text();
    } else {
        m_selectionCopyText = selectedOcrText(QRectF(QPointF(0.0, 0.0), pageSize));
    }

    updateSelectionText();
    viewport()->update();
}

void AtlasPdfSelectableView::copySelectionToClipboard()
{
    if (m_selectionCopyEnabled) {
        QString textToCopy;
        if (!m_selectionCopyText.isEmpty()) {
            textToCopy = m_selectionCopyText;
        } else {
            textToCopy = selectedText();
        }

        if (!textToCopy.trimmed().isEmpty()) {
            QApplication::clipboard()->setText(textToCopy);
            return;
        }
    }

    if (copySelectedEmbeddedImageToClipboard()) {
        return;
    }
}

void AtlasPdfSelectableView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *copyAction = menu.addAction(tr("Copiar seleção"));
    copyAction->setEnabled((m_selectionCopyEnabled && !selectedText().trimmed().isEmpty())
                           || hasSelectedPageArea());

    QAction *selectedAction = menu.exec(event->globalPos());
    if (selectedAction == copyAction) {
        copySelectionToClipboard();
    }
}

bool AtlasPdfSelectableView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == viewport() && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->matches(QKeySequence::SelectAll)) {
            selectAllCurrentPage();
            keyEvent->accept();
            return true;
        }
        if (keyEvent->matches(QKeySequence::Copy)) {
            copySelectionToClipboard();
            keyEvent->accept();
            return true;
        }
    }

    return QPdfView::eventFilter(watched, event);
}

void AtlasPdfSelectableView::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::SelectAll)) {
        selectAllCurrentPage();
        event->accept();
        return;
    }

    if (event->matches(QKeySequence::Copy)) {
        copySelectionToClipboard();
        event->accept();
        return;
    }

    QPdfView::keyPressEvent(event);
}

void AtlasPdfSelectableView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QPdfView::mousePressEvent(event);
        return;
    }

    const QPoint viewportPosition = viewport()->mapFromGlobal(event->globalPosition().toPoint());
    QPointF pagePoint;
    int pageIndex = -1;
    if (!pointToPagePoint(viewportPosition, &pageIndex, &pagePoint)) {
        clearTextSelection();
        QPdfView::mousePressEvent(event);
        return;
    }

    m_draggingSelection = true;
    setFocus(Qt::MouseFocusReason);
    viewport()->setFocus(Qt::MouseFocusReason);
    m_selectionPage = pageIndex;
    m_selectionStart = pagePoint;
    m_selectionEnd = pagePoint;
    m_selectionViewportStart = viewportPosition;
    m_selectionViewportEnd = viewportPosition;
    m_selectionCopyText.clear();
    m_selection.reset();
    emit textSelectionChanged(QString());
    viewport()->update();
    event->accept();
}

void AtlasPdfSelectableView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_draggingSelection) {
        QPdfView::mouseMoveEvent(event);
        return;
    }

    const QPoint viewportPosition = viewport()->mapFromGlobal(event->globalPosition().toPoint());
    m_selectionViewportEnd = viewportPosition;
    updateCurrentSelection(viewportPosition);
    event->accept();
}

void AtlasPdfSelectableView::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_draggingSelection || event->button() != Qt::LeftButton) {
        QPdfView::mouseReleaseEvent(event);
        return;
    }

    const QPoint viewportPosition = viewport()->mapFromGlobal(event->globalPosition().toPoint());
    m_selectionViewportEnd = viewportPosition;
    m_draggingSelection = false;
    updateCurrentSelection(viewportPosition);
    event->accept();
}

void AtlasPdfSelectableView::paintEvent(QPaintEvent *event)
{
    QPdfView::paintEvent(event);

    if (m_selectionPage < 0) {
        return;
    }

    const QVector<PageViewport> viewports = pageViewports();
    const auto pageIt = std::find_if(viewports.cbegin(), viewports.cend(), [this](const PageViewport &page) {
        return page.pageIndex == m_selectionPage;
    });
    if (pageIt == viewports.cend()) {
        return;
    }

    QPainter painter(viewport());

    painter.setRenderHint(QPainter::Antialiasing, false);

    const QVector<QRectF> textRects = visibleSelectionRects(*pageIt);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(75, 155, 245, 115));
    for (const QRectF &rect : textRects) {
        painter.fillRect(QRectF(pageIt->rect.left() + rect.left() * pageIt->scale,
                                pageIt->rect.top() + rect.top() * pageIt->scale,
                                rect.width() * pageIt->scale,
                                rect.height() * pageIt->scale),
                         QColor(75, 155, 245, 115));
    }

    const QVector<QRectF> imageRects = textRects.isEmpty() ? selectedEmbeddedImageRects() : QVector<QRectF>();
    if (!imageRects.isEmpty()) {
        QPen pen(QColor(25, 95, 210, 210));
        pen.setWidth(2);
        painter.setPen(pen);
        painter.setBrush(QColor(40, 120, 255, 45));
        for (const QRectF &rect : imageRects) {
            const QRectF viewRect(pageIt->rect.left() + rect.left() * pageIt->scale,
                                  pageIt->rect.top() + rect.top() * pageIt->scale,
                                  rect.width() * pageIt->scale,
                                  rect.height() * pageIt->scale);
            painter.drawRect(viewRect.adjusted(1.0, 1.0, -1.0, -1.0));
            painter.fillRect(viewRect.adjusted(2.0, 2.0, -2.0, -2.0), QColor(40, 120, 255, 45));
        }
    }
}

void AtlasPdfSelectableView::resizeEvent(QResizeEvent *event)
{
    QPdfView::resizeEvent(event);
    viewport()->update();
}

void AtlasPdfSelectableView::scrollContentsBy(int dx, int dy)
{
    QPdfView::scrollContentsBy(dx, dy);
    viewport()->update();
}

void AtlasPdfSelectableView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        const QPoint angle = event->angleDelta();
        const int delta = angle.y() != 0 ? angle.y() : angle.x();
        if (delta != 0) {
            const qreal currentZoom = zoomMode() == QPdfView::ZoomMode::Custom ? zoomFactor() : 1.0;
            const qreal step = delta > 0 ? 1.1 : 1.0 / 1.1;
            setZoomMode(QPdfView::ZoomMode::Custom);
            setZoomFactor(qBound<qreal>(0.25, currentZoom * step, 3.0));
            emit zoomPercentChanged(qRound(zoomFactor() * 100.0));
            viewport()->update();
            event->accept();
            return;
        }
    }

    QPdfView::wheelEvent(event);
}

QVector<AtlasPdfSelectableView::PageViewport> AtlasPdfSelectableView::pageViewports() const
{
    QVector<PageViewport> pages;
    QPdfDocument *pdfDocument = document();
    if (!pdfDocument || pdfDocument->status() != QPdfDocument::Status::Ready) {
        return pages;
    }

    const QMargins margins = documentMargins();
    const int spacing = pageSpacing();
    const int viewportWidth = viewport()->width();
    const int viewportHeight = viewport()->height();
    const qreal screenScale = logicalDpiY() / 72.0;
    const int firstPage = pageMode() == QPdfView::PageMode::SinglePage && pageNavigator()
                              ? pageNavigator()->currentPage()
                              : 0;
    const int lastPage = pageMode() == QPdfView::PageMode::SinglePage
                             ? firstPage + 1
                             : pdfDocument->pageCount();

    qreal maxScaledWidth = 0.0;
    QVector<qreal> pageScales;
    pageScales.reserve(std::max(0, lastPage - firstPage));

    for (int pageIndex = firstPage; pageIndex < lastPage; ++pageIndex) {
        const QSizeF pointSize = pdfDocument->pagePointSize(pageIndex);
        if (pointSize.isEmpty()) {
            pageScales.push_back(1.0);
            continue;
        }

        qreal scale = zoomFactor() * screenScale;
        if (zoomMode() == QPdfView::ZoomMode::FitToWidth) {
            scale = (viewportWidth - margins.left() - margins.right()) / pointSize.width();
        } else if (zoomMode() == QPdfView::ZoomMode::FitInView) {
            const qreal widthScale = (viewportWidth - margins.left() - margins.right()) / pointSize.width();
            const qreal heightScale = (viewportHeight - margins.top() - margins.bottom()) / pointSize.height();
            scale = std::min(widthScale, heightScale);
        }

        scale = std::max<qreal>(0.1, scale);
        pageScales.push_back(scale);
        maxScaledWidth = std::max(maxScaledWidth, pointSize.width() * scale);
    }

    const qreal documentWidth = margins.left() + maxScaledWidth + margins.right();
    qreal contentY = margins.top();

    for (int pageIndex = firstPage; pageIndex < lastPage; ++pageIndex) {
        const QSizeF pointSize = pdfDocument->pagePointSize(pageIndex);
        const qreal scale = pageScales.value(pageIndex - firstPage, 1.0);
        const QSize scaledSize(qRound(pointSize.width() * scale), qRound(pointSize.height() * scale));
        const qreal contentX = margins.left() + (maxScaledWidth - scaledSize.width()) / 2.0;

        PageViewport page;
        page.pageIndex = pageIndex;
        page.scale = scale;
        page.pointSize = pointSize;
        page.rect = QRect(qRound(contentX - horizontalScrollBar()->value()
                                 + std::max<qreal>(0.0, viewportWidth - documentWidth) / 2.0),
                          qRound(contentY - verticalScrollBar()->value()),
                          scaledSize.width(),
                          scaledSize.height());
        pages.push_back(page);

        contentY += scaledSize.height() + spacing;
    }

    return pages;
}

bool AtlasPdfSelectableView::pointToPagePoint(const QPoint &viewportPosition,
                                              int *pageIndex,
                                              QPointF *pagePoint) const
{
    const QVector<PageViewport> viewports = pageViewports();
    for (const PageViewport &page : viewports) {
        if (!page.rect.contains(viewportPosition)) {
            continue;
        }

        if (pageIndex) {
            *pageIndex = page.pageIndex;
        }
        if (pagePoint) {
            *pagePoint = QPointF((viewportPosition.x() - page.rect.left()) / page.scale,
                                 (viewportPosition.y() - page.rect.top()) / page.scale);
        }
        return true;
    }

    return false;
}

void AtlasPdfSelectableView::updateCurrentSelection(const QPoint &viewportPosition)
{
    if (!document() || m_selectionPage < 0) {
        return;
    }

    QPointF pagePoint;
    int pageIndex = -1;
    if (!pointToPagePoint(viewportPosition, &pageIndex, &pagePoint) || pageIndex != m_selectionPage) {
        return;
    }

    m_selectionEnd = pagePoint;
    m_selection = document()->getSelection(m_selectionPage, m_selectionStart, m_selectionEnd);
    updateSelectionText();
    viewport()->update();
}

void AtlasPdfSelectableView::updateSelectionText()
{
    if (m_draggingSelection) {
        return;
    }

    emit textSelectionChanged(selectedText());
}

QString AtlasPdfSelectableView::selectedVisualText() const
{
    if (!document() || m_selectionPage < 0 || !hasSelectedPageArea()) {
        return {};
    }

    const QVector<PageViewport> viewports = pageViewports();
    const auto pageIt = std::find_if(viewports.cbegin(), viewports.cend(), [this](const PageViewport &page) {
        return page.pageIndex == m_selectionPage;
    });
    if (pageIt == viewports.cend()) {
        return {};
    }

    const QRectF pageRect = selectedVisualPageRect(*pageIt);
    const QString visualText = AtlasPdfTextEngine::visualTextInRect(m_pdfFilePath,
                                                                    document(),
                                                                    m_selectionPage,
                                                                    pageRect);
    return visualText.trimmed().isEmpty() ? selectedOcrText(pageRect) : visualText;
}

QString AtlasPdfSelectableView::selectedOcrText(const QRectF &pageRect) const
{
    QVector<TextBox> selectedBoxes = selectedOcrBoxes(pageRect);
    if (selectedBoxes.isEmpty()) {
        return {};
    }

    const bool characterBoxes = std::all_of(selectedBoxes.cbegin(), selectedBoxes.cend(), [](const TextBox &box) {
        return box.characterBox;
    });

    std::sort(selectedBoxes.begin(), selectedBoxes.end(), [](const TextBox &left, const TextBox &right) {
        if (left.lineNumber != right.lineNumber) {
            return left.lineNumber < right.lineNumber;
        }
        if (left.characterBox && right.characterBox) {
            return left.wordNumber < right.wordNumber;
        }
        return left.pageRect.left() < right.pageRect.left();
    });

    QStringList lines;
    int currentLine = selectedBoxes.first().lineNumber;
    int previousWord = -1;
    QString lineText;
    QRectF previousRect;
    for (const TextBox &box : std::as_const(selectedBoxes)) {
        if (box.lineNumber != currentLine) {
            if (!lineText.trimmed().isEmpty()) {
                lines.push_back(lineText.trimmed());
            }
            lineText.clear();
            previousRect = QRectF();
            currentLine = box.lineNumber;
            previousWord = -1;
        }
        if (!lineText.isEmpty()) {
            if (characterBoxes) {
                const int currentWord = box.wordNumber / 1000;
                if (previousWord >= 0 && currentWord != previousWord) {
                    lineText += QLatin1Char(' ');
                }
            } else {
                lineText += QLatin1Char(' ');
            }
        }
        lineText += box.text;
        previousRect = box.pageRect;
        previousWord = characterBoxes ? box.wordNumber / 1000 : box.wordNumber;
    }
    if (!lineText.trimmed().isEmpty()) {
        lines.push_back(lineText.trimmed());
    }

    return lines.join(QLatin1Char('\n'));
}

QVector<AtlasPdfSelectableView::TextBox> AtlasPdfSelectableView::selectedOcrBoxes(const QRectF &pageRect) const
{
    const QVector<TextBox> boxes = m_ocrTextBoxesByPage.value(m_selectionPage);
    if (boxes.isEmpty() || pageRect.isEmpty()) {
        return {};
    }

    QVector<TextBox> selectedBoxes;
    selectedBoxes.reserve(boxes.size());
    for (const TextBox &box : boxes) {
        const QRectF overlap = box.pageRect.intersected(pageRect);
        if (overlap.isEmpty()) {
            continue;
        }

        const qreal overlapArea = overlap.width() * overlap.height();
        const qreal boxArea = box.pageRect.width() * box.pageRect.height();
        if (boxArea > 0.0 && (overlapArea / boxArea >= 0.18 || pageRect.contains(box.pageRect.center()))) {
            selectedBoxes.push_back(box);
        }
    }

    std::sort(selectedBoxes.begin(), selectedBoxes.end(), [](const TextBox &left, const TextBox &right) {
        if (left.lineNumber != right.lineNumber) {
            return left.lineNumber < right.lineNumber;
        }
        if (left.characterBox && right.characterBox) {
            return left.wordNumber < right.wordNumber;
        }
        return left.pageRect.left() < right.pageRect.left();
    });

    return selectedBoxes;
}

QVector<QRectF> AtlasPdfSelectableView::mergedOcrSelectionRects(const QVector<TextBox> &boxes,
                                                                const QRectF &pageRect) const
{
    QVector<QRectF> merged;
    if (boxes.isEmpty()) {
        return merged;
    }

    int currentLine = boxes.first().lineNumber;
    QRectF currentRect;
    for (const TextBox &box : boxes) {
        const QRectF rect = box.pageRect.normalized().intersected(pageRect);
        if (rect.width() < 0.5 || rect.height() < 0.5) {
            continue;
        }

        if (currentRect.isNull()) {
            currentRect = rect;
            currentLine = box.lineNumber;
            continue;
        }

        if (box.lineNumber != currentLine) {
            merged.push_back(currentRect);
            currentRect = rect;
            currentLine = box.lineNumber;
            continue;
        }

        currentRect = currentRect.united(rect);
    }

    if (!currentRect.isNull()) {
        merged.push_back(currentRect);
    }

    return merged;
}

QVector<QRectF> AtlasPdfSelectableView::mergedTextSelectionRects(const QVector<QRectF> &rects) const
{
    QVector<QRectF> cleanRects;
    cleanRects.reserve(rects.size());
    for (const QRectF &rect : rects) {
        const QRectF normalized = rect.normalized();
        if (normalized.width() >= 0.5 && normalized.height() >= 0.5) {
            cleanRects.push_back(normalized);
        }
    }

    if (cleanRects.size() < 2) {
        return cleanRects;
    }

    std::sort(cleanRects.begin(), cleanRects.end(), [](const QRectF &left, const QRectF &right) {
        const qreal leftCenter = left.center().y();
        const qreal rightCenter = right.center().y();
        const qreal tolerance = std::max<qreal>(2.0, std::min(left.height(), right.height()) * 0.55);
        if (std::abs(leftCenter - rightCenter) > tolerance) {
            return leftCenter < rightCenter;
        }
        return left.left() < right.left();
    });

    QVector<QVector<QRectF>> lines;
    for (const QRectF &rect : std::as_const(cleanRects)) {
        bool added = false;
        for (QVector<QRectF> &line : lines) {
            QRectF lineBounds;
            for (const QRectF &lineRect : std::as_const(line)) {
                lineBounds = lineBounds.isNull() ? lineRect : lineBounds.united(lineRect);
            }

            const qreal yTolerance = std::max<qreal>(2.0, std::min(lineBounds.height(), rect.height()) * 0.65);
            if (std::abs(lineBounds.center().y() - rect.center().y()) <= yTolerance) {
                line.push_back(rect);
                added = true;
                break;
            }
        }

        if (!added) {
            lines.push_back(QVector<QRectF>{rect});
        }
    }

    QVector<QRectF> merged;
    for (QVector<QRectF> &line : lines) {
        std::sort(line.begin(), line.end(), [](const QRectF &left, const QRectF &right) {
            return left.left() < right.left();
        });

        QRectF current = line.first();
        for (int i = 1; i < line.size(); ++i) {
            const QRectF rect = line.at(i);
            const qreal averageHeight = (current.height() + rect.height()) / 2.0;
            const qreal maxJoinGap = std::max<qreal>(2.5, averageHeight * 0.85);
            const bool sameTextRun = rect.left() <= current.right() + maxJoinGap;

            if (sameTextRun) {
                current = current.united(rect);
            } else {
                merged.push_back(current);
                current = rect;
            }
        }
        merged.push_back(current);
    }

    std::sort(merged.begin(), merged.end(), [](const QRectF &left, const QRectF &right) {
        if (std::abs(left.top() - right.top()) > 1.0) {
            return left.top() < right.top();
        }
        return left.left() < right.left();
    });

    return merged;
}

QRectF AtlasPdfSelectableView::selectedPageRect() const
{
    return QRectF(m_selectionStart, m_selectionEnd).normalized();
}

QRectF AtlasPdfSelectableView::selectedVisualPageRect(const PageViewport &page) const
{
    const QRect viewportRect = selectedVisualViewportRect(page);
    if (viewportRect.isEmpty() || page.scale <= 0.0) {
        return selectedPageRect();
    }

    return QRectF(QPointF((viewportRect.left() - page.rect.left()) / page.scale,
                          (viewportRect.top() - page.rect.top()) / page.scale),
                  QPointF((viewportRect.right() - page.rect.left()) / page.scale,
                          (viewportRect.bottom() - page.rect.top()) / page.scale))
        .normalized();
}

QRect AtlasPdfSelectableView::selectedVisualViewportRect(const PageViewport &page) const
{
    return QRect(m_selectionViewportStart, m_selectionViewportEnd).normalized().intersected(page.rect);
}

bool AtlasPdfSelectableView::hasSelectedPageArea() const
{
    if (!document() || m_selectionPage < 0) {
        return false;
    }

    const QVector<PageViewport> viewports = pageViewports();
    const auto pageIt = std::find_if(viewports.cbegin(), viewports.cend(), [this](const PageViewport &page) {
        return page.pageIndex == m_selectionPage;
    });
    if (pageIt != viewports.cend()) {
        const QRectF visualArea = selectedVisualPageRect(*pageIt);
        return visualArea.width() >= 2.0 && visualArea.height() >= 2.0;
    }

    const QRectF area = selectedPageRect();
    return area.width() >= 4.0 && area.height() >= 4.0;
}

bool AtlasPdfSelectableView::copySelectedEmbeddedImageToClipboard()
{
    if (m_pdfFilePath.isEmpty() || !hasSelectedPageArea()) {
        return false;
    }

    const QRectF selectedRect = selectedPageRect();
    const AtlasPdfImageExtractor extractor(m_pdfFilePath);
    const QVector<AtlasPdfImageExtractor::PdfImage> images = extractor.pageImages(m_selectionPage);

    qreal bestOverlap = 0.0;
    QImage bestImage;
    for (const AtlasPdfImageExtractor::PdfImage &image : images) {
        const QRectF overlap = selectedRect.intersected(image.pageRect);
        qreal overlapArea = overlap.width() * overlap.height();
        if (overlapArea <= 0.0 && selectedRect.adjusted(-30.0, -30.0, 30.0, 30.0).intersects(image.pageRect)) {
            overlapArea = 1.0;
        }
        if (overlapArea > bestOverlap) {
            bestOverlap = overlapArea;
            bestImage = image.image;
        }
    }

    if (bestImage.isNull() || bestOverlap <= 0.0) {
        return false;
    }

    QApplication::clipboard()->setImage(bestImage);
    return true;
}

QVector<QRectF> AtlasPdfSelectableView::selectedEmbeddedImageRects() const
{
    QVector<QRectF> rects;
    if (m_pdfFilePath.isEmpty() || !hasSelectedPageArea()) {
        return rects;
    }

    const QRectF selectedRect = selectedPageRect();
    const QRectF expandedSelection = selectedRect.adjusted(-8.0, -8.0, 8.0, 8.0);
    const AtlasPdfImageExtractor extractor(m_pdfFilePath);
    const QVector<AtlasPdfImageExtractor::PdfImage> images = extractor.pageImages(m_selectionPage);

    for (const AtlasPdfImageExtractor::PdfImage &image : images) {
        if (selectedRect.intersects(image.pageRect) || expandedSelection.contains(image.pageRect.center())) {
            rects.push_back(image.pageRect);
        }
    }

    return rects;
}

QVector<QRectF> AtlasPdfSelectableView::visibleSelectionRects(const PageViewport &page) const
{
    QVector<QRectF> rects;
    if (!document() || page.pointSize.isEmpty() || m_selectionPage < 0) {
        return rects;
    }

    const QRectF pageRect(QPointF(0.0, 0.0), page.pointSize);
    const QRectF dragRect = selectedVisualPageRect(page).intersected(pageRect);
    if (dragRect.isEmpty()) {
        return rects;
    }

    QVector<QRectF> imagePageRects;
    if (!m_pdfFilePath.isEmpty()) {
        const AtlasPdfImageExtractor extractor(m_pdfFilePath);
        const QVector<AtlasPdfImageExtractor::PdfImage> images = extractor.pageImages(m_selectionPage);
        for (const AtlasPdfImageExtractor::PdfImage &imageObject : images) {
            if (imageObject.pageRect.intersects(dragRect)) {
                imagePageRects.push_back(imageObject.pageRect.adjusted(-1.0, -1.0, 1.0, 1.0));
            }
        }
    }

    auto mostlyImage = [&imagePageRects](const QRectF &rect) {
        for (const QRectF &imageRect : imagePageRects) {
            const QRectF overlap = rect.intersected(imageRect);
            if (overlap.isEmpty()) {
                continue;
            }
            const qreal overlapArea = overlap.width() * overlap.height();
            const qreal rectArea = rect.width() * rect.height();
            if (rectArea > 0.0 && overlapArea / rectArea > 0.25) {
                return true;
            }
        }
        return false;
    };

    if (!m_selection || !m_selection->isValid()) {
        const QVector<TextBox> selectedBoxes = selectedOcrBoxes(dragRect);
        return mergedOcrSelectionRects(selectedBoxes, pageRect);
    }

    for (const QPolygonF &bound : m_selection->bounds()) {
        QRectF rect = bound.boundingRect().normalized().intersected(pageRect).intersected(dragRect);
        if (rect.width() < 0.5 || rect.height() < 0.5 || mostlyImage(rect)) {
            continue;
        }

        rects.push_back(rect);
    }

    const qreal maxLineHeight = std::min<qreal>(34.0, std::max<qreal>(14.0, page.pointSize.height() * 0.04));
    const bool hasParagraphBox = std::any_of(rects.cbegin(), rects.cend(), [maxLineHeight](const QRectF &rect) {
        return rect.height() > maxLineHeight * 1.45;
    });
    if (hasParagraphBox) {
        const QVector<TextBox> ocrBoxes = m_ocrTextBoxesByPage.value(m_selectionPage);
        QVector<TextBox> selectedOcrTextBoxes;
        for (const TextBox &box : ocrBoxes) {
            if (box.pageRect.intersects(dragRect) && !mostlyImage(box.pageRect)) {
                selectedOcrTextBoxes.push_back(box);
            }
        }
        if (!selectedOcrTextBoxes.isEmpty()) {
            return mergedOcrSelectionRects(selectedOcrTextBoxes, pageRect);
        }
    }

    std::sort(rects.begin(), rects.end(), [](const QRectF &left, const QRectF &right) {
        if (std::abs(left.top() - right.top()) > 1.0) {
            return left.top() < right.top();
        }
        return left.left() < right.left();
    });

    QVector<QRectF> uniqueRects;
    uniqueRects.reserve(rects.size());
    for (const QRectF &rect : std::as_const(rects)) {
        const bool alreadyAdded = std::any_of(uniqueRects.cbegin(),
                                              uniqueRects.cend(),
                                              [&rect](const QRectF &existing) {
            return existing.adjusted(-0.75, -0.75, 0.75, 0.75).contains(rect.center());
        });
        if (!alreadyAdded) {
            uniqueRects.push_back(rect);
        }
    }

    return mergedTextSelectionRects(uniqueRects);
}

QVector<QRectF> AtlasPdfSelectableView::rasterSelectionRects(const PageViewport &page) const
{
    QVector<QRectF> rects;
    if (!document() || page.pointSize.isEmpty() || m_selectionPage < 0) {
        return rects;
    }

    const QRectF pageRect(QPointF(0.0, 0.0), page.pointSize);
    const QRectF selected = selectedVisualPageRect(page).normalized().intersected(pageRect);
    if (selected.isEmpty()) {
        return rects;
    }

    const qreal renderScale = 2.0;
    const QSize imageSize(qMax(1, qRound(page.pointSize.width() * renderScale)),
                          qMax(1, qRound(page.pointSize.height() * renderScale)));
    const QImage image = document()->render(m_selectionPage, imageSize);
    if (image.isNull()) {
        return rects;
    }

    const QRect scanRect(qMax(0, qFloor(selected.left() * renderScale)),
                         qMax(0, qFloor(selected.top() * renderScale)),
                         qMin(image.width(), qCeil(selected.right() * renderScale))
                             - qMax(0, qFloor(selected.left() * renderScale)),
                         qMin(image.height(), qCeil(selected.bottom() * renderScale))
                             - qMax(0, qFloor(selected.top() * renderScale)));
    if (scanRect.isEmpty()) {
        return rects;
    }

    struct RowBand {
        int top = 0;
        int bottom = 0;
        int left = 0;
        int right = 0;
    };

    QVector<RowBand> rawBands;
    RowBand current;
    bool inBand = false;

    for (int y = scanRect.top(); y <= scanRect.bottom(); ++y) {
        int rowLeft = image.width();
        int rowRight = -1;
        int darkPixels = 0;
        for (int x = scanRect.left(); x <= scanRect.right(); ++x) {
            const QColor color = QColor::fromRgba(image.pixel(x, y));
            if (color.alpha() < 20) {
                continue;
            }
            const int luma = qRound(0.2126 * color.red() + 0.7152 * color.green() + 0.0722 * color.blue());
            if (luma < 150) {
                rowLeft = std::min(rowLeft, x);
                rowRight = std::max(rowRight, x);
                ++darkPixels;
            }
        }

        const bool hasInk = darkPixels >= 3;
        if (hasInk && !inBand) {
            current = {y, y, rowLeft, rowRight};
            inBand = true;
        } else if (hasInk) {
            current.bottom = y;
            current.left = std::min(current.left, rowLeft);
            current.right = std::max(current.right, rowRight);
        } else if (inBand) {
            rawBands.push_back(current);
            inBand = false;
        }
    }

    if (inBand) {
        rawBands.push_back(current);
    }

    const qreal maxBandHeight = 42.0 * renderScale;
    QVector<RowBand> bands;
    for (const RowBand &band : std::as_const(rawBands)) {
        if (band.bottom - band.top < 3 || band.bottom - band.top > maxBandHeight || band.right <= band.left) {
            continue;
        }

        if (!bands.isEmpty()) {
            RowBand &previous = bands.last();
            const int gap = band.top - previous.bottom;
            const int previousHeight = previous.bottom - previous.top;
            const int bandHeight = band.bottom - band.top;
            if (gap >= 0 && gap <= std::max(4, std::min(previousHeight, bandHeight))) {
                previous.bottom = band.bottom;
                previous.left = std::min(previous.left, band.left);
                previous.right = std::max(previous.right, band.right);
                continue;
            }
        }

        bands.push_back(band);
    }

    QVector<QRectF> imagePageRects;
    if (!m_pdfFilePath.isEmpty()) {
        const AtlasPdfImageExtractor extractor(m_pdfFilePath);
        const QVector<AtlasPdfImageExtractor::PdfImage> images = extractor.pageImages(m_selectionPage);
        for (const AtlasPdfImageExtractor::PdfImage &imageObject : images) {
            if (imageObject.pageRect.intersects(selected)) {
                imagePageRects.push_back(imageObject.pageRect.adjusted(-1.0, -1.0, 1.0, 1.0));
            }
        }
    }

    auto intersectsImage = [&imagePageRects](const QRectF &rect) {
        for (const QRectF &imageRect : imagePageRects) {
            const QRectF overlap = rect.intersected(imageRect);
            if (overlap.isEmpty()) {
                continue;
            }
            const qreal overlapArea = overlap.width() * overlap.height();
            const qreal rectArea = rect.width() * rect.height();
            if (rectArea > 0.0 && overlapArea / rectArea > 0.35) {
                return true;
            }
        }
        return false;
    };

    for (const RowBand &band : std::as_const(bands)) {
        struct ColumnRun {
            int left = 0;
            int right = 0;
        };

        QVector<ColumnRun> runs;
        ColumnRun currentRun;
        bool inRun = false;
        int emptyColumns = 0;
        const int bandHeight = qMax(1, band.bottom - band.top + 1);
        const int maxJoinGap = std::clamp(qRound(bandHeight * 0.85), qRound(5.0 * renderScale), qRound(18.0 * renderScale));

        for (int x = band.left; x <= band.right; ++x) {
            int darkPixels = 0;
            for (int y = band.top; y <= band.bottom; ++y) {
                const QColor color = QColor::fromRgba(image.pixel(x, y));
                const int luma = qRound(0.2126 * color.red() + 0.7152 * color.green() + 0.0722 * color.blue());
                if (color.alpha() >= 20 && luma < 150) {
                    ++darkPixels;
                }
            }

            const bool hasInk = darkPixels >= 2;
            if (hasInk && !inRun) {
                currentRun = {x, x};
                emptyColumns = 0;
                inRun = true;
            } else if (hasInk) {
                currentRun.right = x;
                emptyColumns = 0;
            } else if (inRun && emptyColumns++ > maxJoinGap) {
                currentRun.right = qMax(currentRun.left, x - emptyColumns);
                runs.push_back(currentRun);
                inRun = false;
                emptyColumns = 0;
            }
        }

        if (inRun) {
            runs.push_back(currentRun);
        }

        for (const ColumnRun &run : std::as_const(runs)) {
            if (run.right - run.left < qRound(4.0 * renderScale)) {
                continue;
            }

            const QRectF rect(QPointF((run.left - 2) / renderScale, (band.top - 2) / renderScale),
                              QPointF((run.right + 2) / renderScale, (band.bottom + 2) / renderScale));
            const QRectF pageRectForRun = rect.normalized().intersected(selected);
            if (!pageRectForRun.isEmpty() && !intersectsImage(pageRectForRun)) {
                rects.push_back(pageRectForRun);
            }
        }
    }

    return rects;
}
