#include "logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>

namespace ncs {
namespace {

QMutex logMutex;
QString directoryOverride;

QString effectiveDirectory()
{
    if (!directoryOverride.isEmpty()) return directoryOverride;
    const QString root = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation);
    return QDir(QDir(root).filePath(QStringLiteral("NCS")))
        .filePath(QStringLiteral("logs"));
}

QString levelText(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug: return QStringLiteral("DEBUG");
    case LogLevel::Info: return QStringLiteral("INFO");
    case LogLevel::Warning: return QStringLiteral("WARN");
    case LogLevel::Error: return QStringLiteral("ERROR");
    }
    return QStringLiteral("INFO");
}

QString sanitized(QString message)
{
    message.replace(QLatin1Char('\n'), QLatin1Char(' '));
    message.replace(QLatin1Char('\r'), QLatin1Char(' '));
    static const QRegularExpression phone(
        QStringLiteral("(?<![0-9])(1[0-9]{2})[0-9]{4}([0-9]{4})(?![0-9])"));
    message.replace(phone, QStringLiteral("\\1****\\2"));
    static const QRegularExpression token(
        QStringLiteral("(?<![A-Fa-f0-9])[A-Fa-f0-9]{64}(?![A-Fa-f0-9])"));
    message.replace(token, QStringLiteral("[token-redacted]"));
    static const QRegularExpression secret(
        QStringLiteral("(?i)(password|session_token|admin_session_token|验证码)\\s*[:=]\\s*\\S+"));
    message.replace(secret, QStringLiteral("\\1=[redacted]"));
    return message;
}

}

bool Logger::write(LogLevel level, const QString &module,
                   const QString &message, QString *error)
{
    QMutexLocker locker(&logMutex);
    QDir directory(effectiveDirectory());
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        if (error) *error = QStringLiteral("无法创建日志目录");
        return false;
    }
    const QString path = directory.filePath(QStringLiteral("app.log"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << QDateTime::currentDateTime().toString(
                  QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
           << QStringLiteral(" [") << levelText(level) << QStringLiteral("] [")
           << sanitized(module) << QStringLiteral("] ") << sanitized(message) << Qt::endl;
    return stream.status() == QTextStream::Ok;
}

void Logger::debug(const QString &module, const QString &message)
{ write(LogLevel::Debug, module, message); }
void Logger::info(const QString &module, const QString &message)
{ write(LogLevel::Info, module, message); }
void Logger::warning(const QString &module, const QString &message)
{ write(LogLevel::Warning, module, message); }
void Logger::error(const QString &module, const QString &message)
{ write(LogLevel::Error, module, message); }

QString Logger::logDirectory()
{
    QMutexLocker locker(&logMutex);
    return effectiveDirectory();
}

void Logger::setDirectoryOverrideForTests(const QString &directory)
{
    QMutexLocker locker(&logMutex);
    directoryOverride = directory;
}

}
