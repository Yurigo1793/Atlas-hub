#include "Logger.h"

#include <QDebug>

Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::info(const QString &message) const
{
    qInfo().noquote() << "[INFO]" << message;
}

void Logger::warning(const QString &message) const
{
    qWarning().noquote() << "[WARN]" << message;
}

void Logger::error(const QString &message) const
{
    qCritical().noquote() << "[ERROR]" << message;
}
