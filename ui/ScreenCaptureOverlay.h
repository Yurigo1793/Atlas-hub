#pragma once

#include <QPoint>
#include <QRect>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;

class ScreenCaptureOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenCaptureOverlay(QWidget *parent = nullptr);

signals:
    void captureFinished(const QString &path);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QPoint selectionStartGlobal;
    QPoint selectionEndGlobal;
    bool selecting = false;
};
