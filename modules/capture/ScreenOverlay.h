#pragma once

#include <QPoint>
#include <QRect>
#include <QWidget>

/**
 * @brief Fullscreen transparent overlay used to select a capture area.
 */
class ScreenOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit ScreenOverlay(QWidget *parent = nullptr);

signals:
    void areaSelected(const QRect &area);
    void selectionCanceled();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QRect normalizedSelection() const;

    bool m_selecting {false};
    QPoint m_startPoint;
    QPoint m_endPoint;
};
