#include "ScreenCapture.h"

#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>

QImage ScreenCapture::captureArea(const QRect &globalArea) const
{
    if (!globalArea.isValid() || globalArea.width() <= 0 || globalArea.height() <= 0) {
        return {};
    }

    const QPoint center = globalArea.center();
    QScreen *targetScreen = QGuiApplication::screenAt(center);
    if (targetScreen == nullptr) {
        targetScreen = QGuiApplication::primaryScreen();
    }

    if (targetScreen == nullptr) {
        return {};
    }

    const QRect screenGeometry = targetScreen->geometry();
    const int localX = globalArea.x() - screenGeometry.x();
    const int localY = globalArea.y() - screenGeometry.y();

    const QPixmap pixmap =
        targetScreen->grabWindow(0, localX, localY, globalArea.width(), globalArea.height());
    const QImage image = pixmap.toImage();

    if (image.isNull()) {
        return {};
    }

    image.save(QStringLiteral("debug_capture.png"));
    return image;
}
