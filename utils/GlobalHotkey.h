#pragma once

#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QList>
#include <QObject>
#include <QString>

class GlobalHotkey : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;

    bool registerShortcut(const QKeySequence &shortcut);
    bool hasConflict() const;
    QString lastError() const;
    void setNativeWindowId(quintptr windowId);
    void unregisterShortcut();

signals:
    void activated();

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    quint32 nativeModifiers(Qt::KeyboardModifiers modifiers) const;
    QList<quint32> nativeKeys(int key) const;

    int m_hotkeyBaseId;
    QList<int> m_registeredHotkeyIds;
    quintptr m_nativeWindowId;
    bool m_hasConflict;
    QString m_lastError;
};
