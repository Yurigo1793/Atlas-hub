#include "HotkeyManager.h"

#include <QtGlobal>

bool HotkeyManager::initialize()
{
    // Placeholder: future implementation will initialize native hotkey hooks.
    return true;
}

bool HotkeyManager::registerHotkey(const QString &id, int keyCode, int modifiers)
{
    Q_UNUSED(id);
    Q_UNUSED(keyCode);
    Q_UNUSED(modifiers);

    // Placeholder: future implementation will call Windows API.
    return false;
}

void HotkeyManager::unregisterAll()
{
    // Placeholder for future native cleanup.
}
