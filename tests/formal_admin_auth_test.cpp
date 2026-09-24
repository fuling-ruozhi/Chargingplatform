#include "database/database_manager.h"
#include "model/business_error.h"
#include "repository/admin_repository.h"
#include "service/admin_service.h"
#include "util/password_hasher.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QTemporaryDir>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(
        directory.filePath(QStringLiteral("admin-auth.db")));
    if (!database.initialize()) return 1;

    QSqlQuery seed(database.connection());
    if (!seed.prepare(QStringLiteral(
            "SELECT password_hash,salt FROM admin WHERE username=:username"))) return 2;
    seed.bindValue(QStringLiteral(":username"), QStringLiteral("admin"));
    if (!seed.exec() || !seed.next()) return 3;
    const QString hash = seed.value(0).toString();
    const QString salt = seed.value(1).toString();
    if (hash.isEmpty() || salt.isEmpty() || hash == QStringLiteral("123456")
        || !ncs::PasswordHasher::verify(QStringLiteral("123456"), salt, hash)) return 4;

    ncs::AdminRepository repository(database);
    ncs::AdminService service(database, repository);
    const auto unknown = service.login(QStringLiteral("missing"), QStringLiteral("wrong"));
    const auto wrong = service.login(QStringLiteral("admin"), QStringLiteral("wrong"));
    if (unknown.success || wrong.success
        || unknown.message != QStringLiteral("账号或密码错误")
        || wrong.message != unknown.message) return 5;
    for (int attempt = 2; attempt <= 4; ++attempt) {
        const auto result = service.login(QStringLiteral("admin"), QStringLiteral("wrong"));
        if (result.success || result.code
            != ncs::BusinessErrorCode::AdminInvalidCredentials) return 6;
    }
    const auto locked = service.login(QStringLiteral("admin"), QStringLiteral("wrong"));
    if (locked.success || locked.code != ncs::BusinessErrorCode::AdminLocked
        || service.retryAfterSeconds(QStringLiteral("admin")) <= 0) return 7;
    const auto blockedCorrect = service.login(
        QStringLiteral("admin"), QStringLiteral("123456"));
    if (blockedCorrect.success
        || blockedCorrect.code != ncs::BusinessErrorCode::AdminLocked) return 8;

    ncs::AdminService restartedService(database, repository);
    const auto success = restartedService.login(
        QStringLiteral("admin"), QStringLiteral("123456"));
    if (!success.success || success.value.username != QStringLiteral("admin")) return 9;
    const auto summary = restartedService.summary(success.value.id);
    if (!summary.success || summary.value.databasePath != database.databasePath()
        || summary.value.totalChargers != 40
        || summary.value.onlineChargers != 35) return 10;
    return 0;
}
