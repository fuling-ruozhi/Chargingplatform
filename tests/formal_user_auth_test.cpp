#include "database/database_manager.h"
#include "repository/user_repository.h"
#include "service/user_service.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QTemporaryDir>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    ncs::DatabaseManager database(directory.filePath(QStringLiteral("users.db")));
    if (!database.initialize()) return 1;
    ncs::UserRepository repository(database);
    qint64 clockMs = 0;
    ncs::UserService service(database, repository, [&clockMs] { return clockMs; });

    if (service.requestOtp(QStringLiteral("1381234")).code
        != ncs::BusinessErrorCode::InvalidPhone) return 2;
    const auto challenge = service.requestOtp(QStringLiteral("13800138000"));
    if (!challenge.success || challenge.value.displayCode.size() != 6
        || challenge.value.cooldownSeconds != 60
        || challenge.value.expiresSeconds != 300) return 3;
    if (service.requestOtp(QStringLiteral("13800138000")).code
        != ncs::BusinessErrorCode::TooFrequent) return 4;
    if (service.loginWithOtp(QStringLiteral("13800138000"), QStringLiteral("000000")).success)
        return 5;

    const auto login = service.loginWithOtp(QStringLiteral("13800138000"),
                                            challenge.value.displayCode);
    if (!login.success || login.value.phone != QStringLiteral("13800138000")
        || login.value.nickname != QStringLiteral("用户8000")
        || login.value.balance != 0.0 || login.value.status != 1) return 6;

    clockMs += 60 * 1000;
    const auto secondChallenge = service.requestOtp(QStringLiteral("13800138000"));
    const auto secondLogin = service.loginWithOtp(QStringLiteral("13800138000"),
                                                   secondChallenge.value.displayCode);
    if (!secondLogin.success || secondLogin.value.id != login.value.id) return 7;

    const auto renamed = service.updateNickname(login.value.id, QStringLiteral("  新昵称  "));
    if (!renamed.success || renamed.value.nickname != QStringLiteral("新昵称")) return 8;
    if (service.updateNickname(login.value.id, QString(21, QLatin1Char('a'))).success
        || service.updateAvatar(login.value.id, QStringLiteral("C:/secret.png")).success)
        return 9;
    const auto avatar = service.updateAvatar(login.value.id,
                                              QStringLiteral("avatars/demo.png"));
    if (!avatar.success || avatar.value.avatarPath != QStringLiteral("avatars/demo.png"))
        return 10;

    if (service.recharge(login.value.id, 0.0).success
        || service.recharge(login.value.id, 10000.001).success) return 11;
    const auto recharge = service.recharge(login.value.id, 10.25);
    const auto smallRecharge = service.recharge(login.value.id, 0.01);
    if (!recharge.success || recharge.value.balanceBefore != 0.0
        || recharge.value.balanceAfter != 10.25 || !smallRecharge.success
        || smallRecharge.value.balanceAfter != 10.26) return 12;
    QSqlQuery log(database.connection());
    if (!log.prepare(QStringLiteral(
            "SELECT COUNT(*),ROUND(SUM(amount),2) FROM recharge_log WHERE user_id=:id")))
        return 13;
    log.bindValue(QStringLiteral(":id"), login.value.id);
    if (!log.exec()
        || !log.next() || log.value(0).toInt() != 2
        || log.value(1).toDouble() != 10.26) return 13;

    if (!log.prepare(QStringLiteral("UPDATE user SET status=0 WHERE id=:id"))) return 14;
    log.bindValue(QStringLiteral(":id"), login.value.id);
    if (!log.exec()) return 14;
    clockMs += 60 * 1000;
    const auto frozenChallenge = service.requestOtp(QStringLiteral("13800138000"));
    if (!frozenChallenge.success
        || service.loginWithOtp(QStringLiteral("13800138000"),
                                frozenChallenge.value.displayCode).code
            != ncs::BusinessErrorCode::UserFrozen) return 15;
    return 0;
}
