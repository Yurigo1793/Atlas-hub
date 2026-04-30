#include "ScreenCaptureOverlay.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QThread>

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

    const QPoint selectionCenter = selectedGlobalRect.center();
    QScreen *screen = QGuiApplication::screenAt(selectionCenter);
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr) {
        emit captureFinished(QString());
        close();
        return;
    }

    QRect screenRect = screen->geometry();
    QRect selectedOnScreen = selectedGlobalRect.translated(-screenRect.topLeft());
    QRect boundedSelection = selectedOnScreen.intersected(QRect(QPoint(0, 0), screenRect.size()));

    if (boundedSelection.isEmpty()) {
        emit captureFinished(QString());
        close();
        return;
    }

    hide();
    QCoreApplication::processEvents();
    QThread::msleep(120);
    QCoreApplication::processEvents();

    QPixmap cropped = screen->grabWindow(
        0,
        boundedSelection.x(),
        boundedSelection.y(),
        boundedSelection.width(),
        boundedSelection.height()
    );
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
