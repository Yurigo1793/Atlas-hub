#include "ScreenCaptureOverlay.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

ScreenCaptureOverlay::ScreenCaptureOverlay(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setCursor(Qt::CrossCursor);
    setAttribute(Qt::WA_DeleteOnClose);
}

void ScreenCaptureOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        selecting = true;
        selectionStartGlobal = event->globalPosition().toPoint();
        selectionEndGlobal = selectionStartGlobal;
        update();
    }
}

void ScreenCaptureOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (selecting) {
        selectionEndGlobal = event->globalPosition().toPoint();
        update();
    }
}

void ScreenCaptureOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (!selecting || event->button() != Qt::LeftButton) {
        return;
    }

    selecting = false;
    selectionEndGlobal = event->globalPosition().toPoint();

    QRect selectedGlobalRect = QRect(selectionStartGlobal, selectionEndGlobal).normalized();
    if (selectedGlobalRect.isEmpty()) {
        emit captureFinished(QString());
        close();
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) {
        emit captureFinished(QString());
        close();
        return;
    }

    QPixmap full = screen->grabWindow(0);
    full.setDevicePixelRatio(screen->devicePixelRatio());

    QRect screenRect = screen->geometry();
    QRect selectedOnScreen = selectedGlobalRect.translated(-screenRect.topLeft());
    QRect boundedSelection = selectedOnScreen.intersected(QRect(QPoint(0, 0), screenRect.size()));

    if (boundedSelection.isEmpty()) {
        emit captureFinished(QString());
        close();
        return;
    }

    qreal dpr = screen->devicePixelRatio();
    QRect sourceRect(
        qRound(boundedSelection.x() * dpr),
        qRound(boundedSelection.y() * dpr),
        qRound(boundedSelection.width() * dpr),
        qRound(boundedSelection.height() * dpr)
    );

    QPixmap cropped = full.copy(sourceRect);
    QString path = QDir::tempPath() + "/atlas_capture.png";
    QFile::remove(path);

    if (!cropped.isNull() && cropped.save(path)) {
        emit captureFinished(path);
    } else {
        emit captureFinished(QString());
    }

    close();
}

void ScreenCaptureOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0, 120));

    QRect selectedRect = QRect(mapFromGlobal(selectionStartGlobal), mapFromGlobal(selectionEndGlobal)).normalized();
    if (!selectedRect.isEmpty()) {
        painter.setPen(QPen(QColor(255, 255, 255), 2));
        painter.setBrush(QColor(255, 255, 255, 30));
        painter.drawRect(selectedRect);
    }
}
