#pragma once

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class OCRWindow;
}
QT_END_NAMESPACE

class OCRWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit OCRWindow(QWidget *parent = nullptr);
    ~OCRWindow() override;

    void setStatusText(const QString &text);
    void setResultText(const QString &text);
    void appendResultText(const QString &text);

signals:
    void captureRequested();
    void backRequested();

private:
    void setupUiConnections();

    Ui::OCRWindow *ui;
};
