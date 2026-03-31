#pragma once

#include <QImage>

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

    QImage captureFullScreen() const;
};
