#pragma once

#include <QString>

/**
 * @brief Application configuration access facade.
 *
 * Centralizes key/value settings operations so all modules use a
 * consistent persistence policy.
 */
class ConfigManager
{
public:
    struct AppSettings
    {
        bool autoCopyEnabled {false};
        bool showOverlay {true};
        int fontSize {11};
    };

    ConfigManager() = default;

    void load();
    QString value(const QString &key, const QString &defaultValue = {}) const;
    void setValue(const QString &key, const QString &value);

    AppSettings appSettings() const;
    void saveAppSettings(const AppSettings &settings);
};
