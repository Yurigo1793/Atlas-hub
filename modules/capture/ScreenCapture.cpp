#include "ScreenCapture.h"

#include <QColor>

QImage ScreenCapture::captureFullScreen() const
{
    // Stub image placeholder until real screen capture implementation is added.
    QImage image(1280, 720, QImage::Format_ARGB32);
    image.fill(QColor(30, 30, 30));
    return image;
}
