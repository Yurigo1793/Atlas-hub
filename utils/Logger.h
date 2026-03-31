#pragma once

#include <QString>

/**
 * @brief Minimal centralized logger.
 *
 * Encapsulates logging output and can be extended later with sinks
 * (file, telemetry, rotating logs).
 */
class Logger
{
public:
    static Logger &instance();

    void info(const QString &message) const;
    void warning(const QString &message) const;
    void error(const QString &message) const;

private:
    Logger() = default;
};
