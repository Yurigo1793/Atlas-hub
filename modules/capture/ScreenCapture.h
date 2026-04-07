#pragma once

#include <QImage>
#include <QRect>

/**
 * @brief Screen capture module.
 *
 * Provides a single responsibility API for obtaining screen snapshots.
 * Current implementation is a stub for early integration and testing.
 */
class ScreenCapture
{
public:
    ScreenCapture() = default;

    QImage captureArea(const QRect &globalArea) const;
};
