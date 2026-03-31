#include "ConfigManager.h"

#include <QSettings>

#include "Logger.h"

void ConfigManager::load()
{
    Logger::instance().info("Configuration subsystem initialized");
}

QString ConfigManager::value(const QString &key, const QString &defaultValue) const
{
    QSettings settings;
    return settings.value(key, defaultValue).toString();
}

void ConfigManager::setValue(const QString &key, const QString &value)
{
    QSettings settings;
    settings.setValue(key, value);
}

ConfigManager::AppSettings ConfigManager::appSettings() const
{
    QSettings settings;

    AppSettings appSettings;
    appSettings.autoCopyEnabled = settings.value(QStringLiteral("app/autoCopyEnabled"), false).toBool();
    appSettings.showOverlay = settings.value(QStringLiteral("app/showOverlay"), true).toBool();
    appSettings.fontSize = settings.value(QStringLiteral("app/fontSize"), 11).toInt();

    return appSettings;
}

void ConfigManager::saveAppSettings(const ConfigManager::AppSettings &settings)
{
    QSettings qsettings;
    qsettings.setValue(QStringLiteral("app/autoCopyEnabled"), settings.autoCopyEnabled);
    qsettings.setValue(QStringLiteral("app/showOverlay"), settings.showOverlay);
    qsettings.setValue(QStringLiteral("app/fontSize"), settings.fontSize);
}
