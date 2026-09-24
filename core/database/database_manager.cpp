#include "database_manager.h"

#include "database_migration_manager.h"
#include "schema_initializer.h"
#include "seed_data.h"
#include "util/logger.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

namespace ncs {
namespace {

QString databaseFailure(const QString &detail)
{
    const QString lower = detail.toLower();
    if (lower.contains(QStringLiteral("malformed"))
        || lower.contains(QStringLiteral("not a database"))) {
        return QStringLiteral(
            "数据库文件可能已损坏，程序未继续写入。请关闭程序并从 migration-backups "
            "目录恢复最近备份，或保留原文件后联系维护人员。详细信息：%1").arg(detail);
    }
    return detail;
}

}

DatabaseManager::DatabaseManager(const QString &databasePath)
    : databasePath_(databasePath.isEmpty() ? defaultDatabasePath() : databasePath),
      connectionName_(QStringLiteral("ncs_database_%1").arg(
          QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

DatabaseManager::~DatabaseManager()
{
    close();
}

QString DatabaseManager::defaultDatabasePath()
{
    const QString dataRoot = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation);
    return QDir(QDir(dataRoot).filePath(QStringLiteral("NCS")))
        .filePath(QStringLiteral("charge_platform.db"));
}

bool DatabaseManager::open()
{
    lastError_.clear();
    if (database_.isValid() && database_.isOpen()) {
        return configureConnection();
    }

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        setError(QStringLiteral("SQLite driver QSQLITE is unavailable."));
        return false;
    }

    const QFileInfo fileInfo(databasePath_);
    QDir parentDirectory(fileInfo.absolutePath());
    if (!parentDirectory.exists() && !parentDirectory.mkpath(QStringLiteral("."))) {
        setError(QStringLiteral("Cannot create database directory: %1")
                     .arg(parentDirectory.absolutePath()));
        return false;
    }

    database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database_.setDatabaseName(databasePath_);
    if (!database_.open()) {
        setError(databaseFailure(QStringLiteral("无法打开 SQLite 数据库：%1")
                     .arg(database_.lastError().text())));
        return false;
    }
    return configureConnection();
}

void DatabaseManager::close()
{
    if (!database_.isValid()) {
        return;
    }

    database_.close();
    database_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName_);
}

bool DatabaseManager::configureConnection()
{
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        setError(databaseFailure(QStringLiteral("无法启用外键约束：%1")
                     .arg(query.lastError().text())));
        return false;
    }
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys")) || !query.next()
        || query.value(0).toInt() != 1) {
        setError(databaseFailure(QStringLiteral("SQLite 外键约束未启用：%1")
                     .arg(query.lastError().text())));
        return false;
    }

    if (!query.exec(QStringLiteral("PRAGMA journal_mode = WAL")) || !query.next()) {
        setError(databaseFailure(QStringLiteral("无法启用 WAL 模式：%1")
                     .arg(query.lastError().text())));
        return false;
    }
    if (query.value(0).toString().compare(QStringLiteral("wal"), Qt::CaseInsensitive) != 0) {
        setError(QStringLiteral("SQLite did not activate WAL mode; actual mode is %1.")
                     .arg(query.value(0).toString()));
        return false;
    }
    return true;
}

bool DatabaseManager::initialize()
{
    if (!open()) {
        return false;
    }

    if (database_.tables().contains(QStringLiteral("schema_version"))) {
        const int version = schemaVersion();
        if (version < 1 || version > CurrentSchemaVersion) {
            setError(QStringLiteral("Unsupported schema version %1; latest is %2.")
                         .arg(version).arg(CurrentSchemaVersion));
            return false;
        }
        QString error;
        if (version < CurrentSchemaVersion) {
            QString backupPath;
            if (!createMigrationBackup(&backupPath)) return false;
            if (!DatabaseMigrationManager::migrate(database_, version, &error)) {
                setError(QStringLiteral("%1 Migration backup: %2")
                             .arg(error, backupPath));
                return false;
            }
        } else if (!DatabaseMigrationManager::validate(database_, &error)) {
            // A historical release could record a newer version before all columns existed.
            // Re-enter the transactional migration path after backing up so known schema
            // drift can be repaired without dropping any business data.
            QString backupPath;
            if (!createMigrationBackup(&backupPath)) return false;
            if (!DatabaseMigrationManager::migrate(database_, version, &error)) {
                setError(QStringLiteral("%1 Migration backup: %2")
                             .arg(error, backupPath));
                return false;
            }
        }
        if (!DatabaseMigrationManager::validate(database_, &error)) {
            setError(error);
            return false;
        }
        if (!transaction()) {
            return false;
        }
        if (!SeedData::populate(database_, &error)) {
            rollback();
            setError(error);
            return false;
        }
        if (!commit()) {
            rollback();
            return false;
        }
        return true;
    }
    return createInitialDatabase();
}

bool DatabaseManager::createInitialDatabase()
{
    if (!transaction()) {
        return false;
    }

    QString error;
    if (!SchemaInitializer::apply(database_, &error)) {
        rollback();
        setError(error);
        return false;
    }
    QSqlQuery versionQuery(database_);
    if (!versionQuery.prepare(QStringLiteral(
            "INSERT INTO schema_version(version, applied_at) VALUES(:version, :applied_at)"))) {
        rollback();
        setError(versionQuery.lastError().text());
        return false;
    }
    versionQuery.bindValue(QStringLiteral(":version"), CurrentSchemaVersion);
    versionQuery.bindValue(QStringLiteral(":applied_at"),
                           QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (!versionQuery.exec()) {
        const QString message = QStringLiteral("Cannot record schema version: %1")
                                    .arg(versionQuery.lastError().text());
        rollback();
        setError(message);
        return false;
    }
    if (!DatabaseMigrationManager::validate(database_, &error)) {
        rollback();
        setError(error);
        return false;
    }

    if (!SeedData::populate(database_, &error)) {
        rollback();
        setError(error);
        return false;
    }
    if (!commit()) {
        rollback();
        return false;
    }
    return true;
}

bool DatabaseManager::createMigrationBackup(QString *backupPath)
{
    backupPath->clear();
    if (databasePath_ == QStringLiteral(":memory:")) return true;

    const QFileInfo source(databasePath_);
    QDir backupDirectory(source.absoluteDir().filePath(
        QStringLiteral("migration-backups")));
    if (!backupDirectory.exists() && !backupDirectory.mkpath(QStringLiteral("."))) {
        setError(QStringLiteral("Cannot create migration backup directory: %1")
                     .arg(backupDirectory.absolutePath()));
        return false;
    }

    const QString timestamp = QDateTime::currentDateTimeUtc().toString(
        QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString unique = QUuid::createUuid().toString(QUuid::WithoutBraces);
    *backupPath = backupDirectory.filePath(QStringLiteral("%1-v%2-%3-%4.db")
        .arg(source.completeBaseName())
        .arg(schemaVersion())
        .arg(timestamp, unique));

    QSqlQuery backup(database_);
    if (!backup.prepare(QStringLiteral("VACUUM INTO :backup_path"))) {
        setError(QStringLiteral("Cannot prepare migration backup: %1")
                     .arg(backup.lastError().text()));
        return false;
    }
    backup.bindValue(QStringLiteral(":backup_path"), *backupPath);
    if (!backup.exec()) {
        setError(QStringLiteral("Cannot create migration backup: %1")
                     .arg(backup.lastError().text()));
        return false;
    }
    return true;
}

bool DatabaseManager::transaction()
{
    if (!database_.isOpen() && !open()) {
        return false;
    }
    if (!database_.transaction()) {
        setError(QStringLiteral("Cannot start database transaction: %1")
                     .arg(database_.lastError().text()));
        return false;
    }
    return true;
}

bool DatabaseManager::commit()
{
    if (!database_.commit()) {
        setError(QStringLiteral("Cannot commit database transaction: %1")
                     .arg(database_.lastError().text()));
        return false;
    }
    return true;
}

bool DatabaseManager::rollback()
{
    if (!database_.rollback()) {
        setError(QStringLiteral("Cannot roll back database transaction: %1")
                     .arg(database_.lastError().text()));
        return false;
    }
    return true;
}

int DatabaseManager::schemaVersion() const
{
    if (!database_.isOpen()) {
        return -1;
    }
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral("SELECT version FROM schema_version ORDER BY version DESC LIMIT 1"))
        || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

QSqlDatabase DatabaseManager::connection() const
{
    return database_;
}

QString DatabaseManager::databasePath() const
{
    return databasePath_;
}

QString DatabaseManager::lastError() const
{
    return lastError_;
}

void DatabaseManager::setError(const QString &message)
{
    lastError_ = message;
    Logger::error(QStringLiteral("database"), message);
}

}  // namespace ncs
