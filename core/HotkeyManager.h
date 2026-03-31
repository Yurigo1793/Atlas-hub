#pragma once

#include <QString>

/**
 * @brief Interface layer for global hotkeys.
 *
 * This class intentionally keeps implementation minimal for now. It defines
 * extension points for future Windows API registration while keeping controller
 * and UI code independent from platform details.
 */
class HotkeyManager
{
public:
    HotkeyManager() = default;

    bool initialize();
    bool registerHotkey(const QString &id, int keyCode, int modifiers);
    void unregisterAll();
};
