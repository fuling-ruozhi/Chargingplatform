#include "reminder_controller.h"
#include "service/user_client_facade.h"
#include "station_detail_dialog.h"
#include "user_preference_dialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QDate>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTimeEdit>

namespace {

bool check(bool condition, const char *message)
{
    if (!condition) qWarning("FAILED: %s", message);
    return condition;
}

}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    ncs::UserClientFacade facade;

    ncs::UserPreferenceDialog noLocation(facade, false, 116.3, 39.9);
    if (!check(noLocation.findChild<QDoubleSpinBox *>("homeRadius")->minimum() == 0.1,
               "radius lower bound")) return 1;
    const auto setHomeButtons = noLocation.findChildren<QPushButton *>();
    QPushButton *setHome = nullptr;
    for (QPushButton *button : setHomeButtons) {
        if (button->text().contains(QStringLiteral("设为家"))) setHome = button;
    }
    if (!check(setHome && !setHome->isEnabled(), "unlocated home button disabled")) return 2;
    if (!check(noLocation.findChild<QCheckBox *>("slowChargerType") != nullptr
                   && noLocation.findChild<QCheckBox *>("fastChargerType") != nullptr,
               "charger type controls")) return 3;
    if (!check(noLocation.findChild<QTimeEdit *>("reminderStart")->time()
                   == QTime(8, 0), "default reminder time")) return 4;

    ncs::UserPreferenceDialog located(facade, true, 116.3220, 39.9623);
    QPushButton *locatedSetHome = nullptr;
    for (QPushButton *button : located.findChildren<QPushButton *>()) {
        if (button->text().contains(QStringLiteral("设为家"))) locatedSetHome = button;
    }
    if (!check(locatedSetHome && locatedSetHome->isEnabled(), "located home button enabled")) return 5;
    locatedSetHome->click();
    bool homeUpdated = false;
    for (QLabel *label : located.findChildren<QLabel *>()) {
        if (label->text().contains(QStringLiteral("39.962300"))) homeUpdated = true;
    }
    if (!check(homeUpdated, "current location copied into form")) return 6;
    ncs::UserPreference stalePreference;
    facade.preferenceReceived(stalePreference);
    homeUpdated = false;
    for (QLabel *label : located.findChildren<QLabel *>()) {
        if (label->text().contains(QStringLiteral("39.962300"))) homeUpdated = true;
    }
    if (!check(homeUpdated, "late preference response does not overwrite home edit")) return 20;
    QPushButton *clearHome = nullptr;
    for (QPushButton *button : located.findChildren<QPushButton *>()) {
        if (button->text().contains(QStringLiteral("清除家位置"))) clearHome = button;
    }
    if (!check(clearHome && clearHome->isEnabled(), "clear home enabled after set")) return 21;
    clearHome->click();
    bool homeCleared = false;
    for (QLabel *label : located.findChildren<QLabel *>()) {
        if (label->text() == QStringLiteral("未设置")) homeCleared = true;
    }
    if (!check(homeCleared && !clearHome->isEnabled(), "clear home refreshes form")) return 22;

    ncs::ReminderEdgeState edge;
    ncs::ReminderMatch match;
    match.stationId = 7;
    match.stationName = QStringLiteral("测试站");
    match.idleMatchedChargers = 2;
    const QDateTime first(QDate(2026, 9, 8), QTime(8, 0));
    if (!check(edge.update({match}, first).size() == 1, "first match notifies")) return 7;
    if (!check(edge.update({match}, first.addSecs(60)).isEmpty(), "active match is edge-triggered")) return 8;
    if (!check(edge.update({}, first.addSecs(120)).isEmpty(), "inactive clears active state")) return 9;
    if (!check(edge.update({match}, first.addSecs(5 * 60)).isEmpty(), "cooldown suppresses rebound")) return 10;
    edge.update({}, first.addSecs(11 * 60));
    if (!check(edge.update({match}, first.addSecs(11 * 60 + 1)).size() == 1,
               "cooldown allows later rebound")) return 11;

    ncs::ReminderController controller(facade);
    controller.setEnabled(true);
    if (!check(controller.timerActive(), "enabled controller starts timer")) return 12;
    facade.disconnected();
    if (!check(!controller.timerActive() && !controller.requestPending(),
               "disconnect stops timer and clears pending")) return 13;
    controller.setEnabled(true);
    facade.networkError(QStringLiteral("temporary network failure"));
    if (!check(controller.timerActive(), "transient network failure keeps timer")) return 14;
    facade.sessionInvalid();
    if (!check(!controller.timerActive(), "session invalid stops timer")) return 15;
    ncs::ReminderController freshController(facade);
    freshController.setEnabled(true);
    if (!check(freshController.timerActive(), "new enabled session can restart timer")) return 16;

    ncs::StationDetail detail;
    detail.station.id = 42;
    detail.station.name = QStringLiteral("测试电站");
    ncs::StationDetailDialog stationDialog(detail, 1, facade);
    QPushButton *favoriteButton = nullptr;
    for (QPushButton *button : stationDialog.findChildren<QPushButton *>()) {
        if (button->text().contains(QStringLiteral("关注"))) favoriteButton = button;
    }
    if (!check(favoriteButton && favoriteButton->text().contains(QStringLiteral("☆")),
               "station starts unfollowed")) return 17;
    facade.favoriteStationChanged(42, true);
    if (!check(favoriteButton->text().contains(QStringLiteral("★")),
               "favorite success updates station detail")) return 18;
    facade.favoriteStationChanged(42, false);
    if (!check(favoriteButton->text().contains(QStringLiteral("☆")),
               "unfavorite success updates station detail")) return 19;
    return 0;
}
