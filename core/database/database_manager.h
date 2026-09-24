#pragma once

#include <QSqlDatabase>
#include <QString>

namespace ncs {

class DatabaseTestProbe;

class DatabaseManager
{
public:
    static constexpr int CurrentSchemaVersion = 10;

    explicit DatabaseManager(const QString &databasePath = QString());
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    bool open();
    void close();
    bool initialize();

    bool transaction();
    bool commit();
    bool rollback();

    int schemaVersion() const;
    QSqlDatabase connection() const;
    QString databasePath() const;
    QString lastError() const;

    static QString defaultDatabasePath();

private:
    friend class DatabaseTestProbe;

    bool configureConnection();
    bool createInitialDatabase();
    bool createMigrationBackup(QString *backupPath);
    void setError(const QString &message);

    QString databasePath_;
    QString connectionName_;
    QSqlDatabase database_;
    QString lastError_;
};

}  // namespace ncs
