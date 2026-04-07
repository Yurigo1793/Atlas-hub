#include "ScreenOverlay.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

ScreenOverlay::ScreenOverlay(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    setAttribute(Qt::WA_TranslucentBackground, true);

    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        setGeometry(screen->virtualGeometry());
    }
}

void ScreenOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.fillRect(rect(), QColor(0, 0, 0, 95));

    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setPen(QPen(QColor(255, 255, 255, 220), 1));
    painter.drawText(QPoint(20, 32), QStringLiteral("Arraste para selecionar"));
    painter.drawText(QPoint(20, 52), QStringLiteral("ESC para cancelar"));

    const QRect selectionRect = normalizedSelection();
    if (!selectionRect.isNull()) {
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.fillRect(selectionRect, Qt::transparent);

        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setPen(QPen(QColor(0, 120, 215), 2));
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
    event->accept();
    update();
}

void ScreenOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_selecting) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    m_endPoint = mapFromGlobal(event->globalPosition().toPoint());
    event->accept();
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
        event->accept();
        emit areaSelected(QRect(mapToGlobal(selectionRect.topLeft()), selectionRect.size()));
    } else {
        event->accept();
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
