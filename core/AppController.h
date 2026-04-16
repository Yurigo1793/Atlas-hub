#pragma once

#include <QObject>

class MainWindow;

class AppController : public QObject
{
    Q_OBJECT

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    void initialize();

private:
    MainWindow *m_mainWindow;
};
