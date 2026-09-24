#include "database/database_manager.h"
#include "util/logger.h"

#include <QCoreApplication>
#include <QDate>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>

namespace {

QString readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    return QString::fromUtf8(file.readAll());
}

bool ignoredPath(const QString &path)
{
    const QString normalized = QDir::fromNativeSeparators(path);
    return normalized.contains(QStringLiteral("/.git/"))
        || normalized.contains(QStringLiteral("/build/"))
        || normalized.contains(QStringLiteral("/build-"))
        || normalized.contains(QStringLiteral("/artifacts/"));
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::Logger::setDirectoryOverrideForTests(directory.filePath(QStringLiteral("logs")));
    const QString token(64, QLatin1Char('a'));
    QString logError;
    if (!ncs::Logger::write(ncs::LogLevel::Warning, QStringLiteral("nfr-test"),
            QStringLiteral("phone=13800138000 password=secret session_token=%1\nnext")
                .arg(token), &logError)) return 1;
    const QString logPath = QDir(ncs::Logger::logDirectory()).filePath(QStringLiteral("app.log"));
    const QString log = readFile(logPath);
    if (!log.contains(QStringLiteral("[WARN] [nfr-test]"))
        || !log.contains(QStringLiteral("138****8000"))
        || log.contains(QStringLiteral("13800138000"))
        || log.contains(token) || log.contains(QStringLiteral("secret"))
        || log.count(QLatin1Char('\n')) != 1) return 2;

    const QString corruptPath = directory.filePath(QStringLiteral("corrupt.db"));
    QFile corrupt(corruptPath);
    if (!corrupt.open(QIODevice::WriteOnly)
        || corrupt.write("this is not sqlite") < 0) return 3;
    corrupt.close();
    ncs::DatabaseManager database(corruptPath);
    if (database.initialize()
        || !database.lastError().contains(QStringLiteral("损坏"))
        || !database.lastError().contains(QStringLiteral("migration-backups"))) return 4;

    const QString root = QStringLiteral(NCS_SOURCE_ROOT);
    const QStringList sourceRoots{QStringLiteral("core"), QStringLiteral("client_user"),
                                  QStringLiteral("client_admin"), QStringLiteral("tests")};
    for (const QString &relative : sourceRoots) {
        QDirIterator files(QDir(root).filePath(relative),
            {QStringLiteral("*.cpp"), QStringLiteral("*.h")}, QDir::Files,
            QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const QString content = readFile(files.next());
            if (content.count(QLatin1Char('\n')) + 1 > 400) return 5;
        }
    }

    static const QRegularExpression uiSql(
        QStringLiteral("\"\\s*(SELECT|INSERT|UPDATE|DELETE|PRAGMA)\\s+"),
        QRegularExpression::CaseInsensitiveOption);
    for (const QString &relative : {QStringLiteral("client_user"),
                                    QStringLiteral("client_admin")}) {
        QDirIterator files(QDir(root).filePath(relative),
            {QStringLiteral("*.cpp"), QStringLiteral("*.h")}, QDir::Files,
            QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const QString content = readFile(files.next());
            if (uiSql.match(content).hasMatch()) return 6;
        }
    }

    static const QString ipv4Octet = QStringLiteral(
        "(?:25[0-5]|2[0-4]\\d|1?\\d?\\d)");
    static const QRegularExpression privateIpv4(QStringLiteral(
        "\\b(?:10\\.%1\\.%1\\.%1|172\\.(?:1[6-9]|2\\d|3[01])\\.%1\\.%1|"
        "192\\.168\\.%1\\.%1)\\b").arg(ipv4Octet));
    QDirIterator repository(root, QDir::Files, QDirIterator::Subdirectories);
    while (repository.hasNext()) {
        const QString path = repository.next();
        if (ignoredPath(path)) continue;
        const QString content = readFile(path);
        const QString personalPath = QStringLiteral("C:")
            + QStringLiteral("\\Users\\");
        if (privateIpv4.match(content).hasMatch()
            || content.contains(personalPath)) return 7;
    }
    const QString verify = readFile(QDir(root).filePath(
        QStringLiteral("scripts/linux_verify.ps1")));
    if (verify.contains(QStringLiteral("[string]$RemoteHost ="))) return 8;
    return 0;
}
