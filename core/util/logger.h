#pragma once

#include <QString>

namespace ncs {

enum class LogLevel { Debug, Info, Warning, Error };

class Logger
{
public:
    static bool write(LogLevel level, const QString &module,
                      const QString &message, QString *error = nullptr);
    static void debug(const QString &module, const QString &message);
    static void info(const QString &module, const QString &message);
    static void warning(const QString &module, const QString &message);
    static void error(const QString &module, const QString &message);
    static QString logDirectory();

    // Test isolation only; production uses QStandardPaths.
    static void setDirectoryOverrideForTests(const QString &directory);
};

}
