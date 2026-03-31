#include "ScreenOverlay.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

ScreenOverlay::ScreenOverlay(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowFlag(Qt::Tool, true);
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    setWindowState(Qt::WindowFullScreen);

    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        setGeometry(screen->geometry());
    }
}

void ScreenOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.fillRect(rect(), QColor(0, 0, 0, 120));

    const QRect selectionRect = normalizedSelection();
    if (!selectionRect.isNull()) {
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(selectionRect, Qt::transparent);

        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setPen(QPen(QColor(0, 170, 255), 2));
        painter.drawRect(selectionRect);
    }
}

void ScreenOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_selecting = true;
    m_startPoint = mapFromGlobal(event->globalPosition().toPoint());
    m_endPoint = m_startPoint;
    update();
}

void ScreenOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_selecting) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    m_endPoint = mapFromGlobal(event->globalPosition().toPoint());
    update();
}

void ScreenOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_selecting || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    m_selecting = false;
    m_endPoint = mapFromGlobal(event->globalPosition().toPoint());

    const QRect selectionRect = normalizedSelection();
    if (selectionRect.isValid() && selectionRect.width() > 2 && selectionRect.height() > 2) {
        emit areaSelected(QRect(mapToGlobal(selectionRect.topLeft()), selectionRect.size()));
    } else {
        emit selectionCanceled();
    }
}

void ScreenOverlay::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit selectionCanceled();
        return;
    }

    QWidget::keyPressEvent(event);
}

QRect ScreenOverlay::normalizedSelection() const
{
    return QRect(m_startPoint, m_endPoint).normalized();
}
