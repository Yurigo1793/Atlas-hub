#include "GlobalHotkey.h"

#include <QCoreApplication>
#include <QKeyCombination>
#include <QMetaObject>
#include <QStringList>

#include <utility>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
int nextHotkeyBaseId()
{
    static int nextId = 0x4000;
    const int id = nextId;
    nextId += 8;
    return id;
}

#ifdef Q_OS_WIN
QString windowsErrorText(DWORD errorCode)
{
    if (errorCode == 0) {
        return QString();
    }

    wchar_t *buffer = nullptr;
    const DWORD size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER
                                          | FORMAT_MESSAGE_FROM_SYSTEM
                                          | FORMAT_MESSAGE_IGNORE_INSERTS,
                                      nullptr,
                                      errorCode,
                                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                      reinterpret_cast<LPWSTR>(&buffer),
                                      0,
                                      nullptr);

    QString message = size > 0 && buffer != nullptr
                          ? QString::fromWCharArray(buffer, static_cast<int>(size)).trimmed()
                          : QStringLiteral("Windows error %1").arg(errorCode);

    if (buffer != nullptr) {
        LocalFree(buffer);
    }

    return message;
}
#endif
}

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
    , m_hotkeyBaseId(nextHotkeyBaseId())
    , m_nativeWindowId(0)
    , m_hasConflict(false)
{
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterShortcut();

    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
}

bool GlobalHotkey::registerShortcut(const QKeySequence &shortcut)
{
    unregisterShortcut();
    m_hasConflict = false;
    m_lastError.clear();

    if (shortcut.isEmpty()) {
        m_lastError = tr("Atalho vazio.");
        return false;
    }

#ifdef Q_OS_WIN
    const QKeyCombination keyCombination = shortcut[0];
    const quint32 modifiers = nativeModifiers(keyCombination.keyboardModifiers());
    const QList<quint32> keys = nativeKeys(keyCombination.key());

    if (keys.isEmpty()) {
        m_lastError = tr("Tecla não suportada.");
        return false;
    }

    HWND targetWindow = reinterpret_cast<HWND>(m_nativeWindowId);
    QStringList errors;
    bool failed = false;

    for (int i = 0; i < keys.size(); ++i) {
        const int hotkeyId = m_hotkeyBaseId + i;
        if (RegisterHotKey(targetWindow,
                           hotkeyId,
                           modifiers | MOD_NOREPEAT,
                           keys.at(i))
            != 0) {
            m_registeredHotkeyIds.append(hotkeyId);
        } else {
            failed = true;
            const DWORD errorCode = GetLastError();
            if (errorCode == ERROR_HOTKEY_ALREADY_REGISTERED) {
                m_hasConflict = true;
                errors << tr("Atalho global já está sendo usado por outro aplicativo.");
            } else {
                errors << windowsErrorText(errorCode);
            }
        }
    }

    if (!failed && !m_registeredHotkeyIds.isEmpty()) {
        return true;
    }

    unregisterShortcut();
    m_lastError = errors.isEmpty() ? tr("Falha desconhecida.") : errors.join(QStringLiteral(" | "));
    return false;
#else
    Q_UNUSED(shortcut)
    m_lastError = tr("Atalho global não é suportado nesta plataforma.");
    return false;
#endif
}

bool GlobalHotkey::hasConflict() const
{
    return m_hasConflict;
}

QString GlobalHotkey::lastError() const
{
    return m_lastError;
}

void GlobalHotkey::setNativeWindowId(quintptr windowId)
{
    m_nativeWindowId = windowId;
}

void GlobalHotkey::unregisterShortcut()
{
#ifdef Q_OS_WIN
    HWND targetWindow = reinterpret_cast<HWND>(m_nativeWindowId);
    for (int hotkeyId : std::as_const(m_registeredHotkeyIds)) {
        UnregisterHotKey(targetWindow, hotkeyId);
    }
    m_registeredHotkeyIds.clear();
#endif
}

bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result)

#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") {
        return false;
    }

    const MSG *msg = static_cast<MSG *>(message);
    if (msg && msg->message == WM_HOTKEY && m_registeredHotkeyIds.contains(static_cast<int>(msg->wParam))) {
        QMetaObject::invokeMethod(this, &GlobalHotkey::activated, Qt::QueuedConnection);
        return true;
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
#endif

    return false;
}

quint32 GlobalHotkey::nativeModifiers(Qt::KeyboardModifiers modifiers) const
{
    quint32 native = 0;

#ifdef Q_OS_WIN
    if (modifiers.testFlag(Qt::ControlModifier)) {
        native |= MOD_CONTROL;
    }

    if (modifiers.testFlag(Qt::AltModifier)) {
        native |= MOD_ALT;
    }

    if (modifiers.testFlag(Qt::ShiftModifier)) {
        native |= MOD_SHIFT;
    }

    if (modifiers.testFlag(Qt::MetaModifier)) {
        native |= MOD_WIN;
    }
#else
    Q_UNUSED(modifiers)
#endif

    return native;
}

QList<quint32> GlobalHotkey::nativeKeys(int key) const
{
    QList<quint32> keys;

#ifdef Q_OS_WIN
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        keys << static_cast<quint32>('A' + key - Qt::Key_A);
        return keys;
    }

    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        keys << static_cast<quint32>('0' + key - Qt::Key_0);
        return keys;
    }

    switch (key) {
    case Qt::Key_Backslash:
        keys << VK_OEM_5 << VK_OEM_102;
        break;
    case Qt::Key_Slash:
        keys << VK_OEM_2;
        break;
    case Qt::Key_Semicolon:
        keys << VK_OEM_1;
        break;
    case Qt::Key_Plus:
        keys << VK_OEM_PLUS;
        break;
    case Qt::Key_Minus:
        keys << VK_OEM_MINUS;
        break;
    case Qt::Key_Comma:
        keys << VK_OEM_COMMA;
        break;
    case Qt::Key_Period:
        keys << VK_OEM_PERIOD;
        break;
    case Qt::Key_Space:
        keys << VK_SPACE;
        break;
    case Qt::Key_Escape:
        keys << VK_ESCAPE;
        break;
    case Qt::Key_F1:
    case Qt::Key_F2:
    case Qt::Key_F3:
    case Qt::Key_F4:
    case Qt::Key_F5:
    case Qt::Key_F6:
    case Qt::Key_F7:
    case Qt::Key_F8:
    case Qt::Key_F9:
    case Qt::Key_F10:
    case Qt::Key_F11:
    case Qt::Key_F12:
        keys << static_cast<quint32>(VK_F1 + key - Qt::Key_F1);
        break;
    default:
        break;
    }
#else
    Q_UNUSED(key)
#endif

    return keys;
}
