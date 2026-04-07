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

    const QPixmap pixmap = targetScreen->grabWindow(
        0, globalArea.x(), globalArea.y(), globalArea.width(), globalArea.height());
    const QImage image = pixmap.toImage();
    image.save(QStringLiteral("debug_capture.png"));
    return image;
}
