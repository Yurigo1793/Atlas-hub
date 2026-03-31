#pragma once

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void setResultText(const QString &text);
    void appendResultText(const QString &text);
    void setStatusText(const QString &text);

signals:
    void ocrRequested();
    void settingsRequested();

private:
    void setupUiConnections();

    Ui::MainWindow *ui;
};
