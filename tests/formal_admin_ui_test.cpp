#include "admin_login_window.h"
#include "admin_main_window.h"
#include "pages/charger_page.h"
#include "pages/charger_status_page.h"
#include "pages/revenue_page.h"
#include "pages/station_page.h"
#include "pages/user_page.h"
#include "model/business_error.h"
#include "service/admin_client_facade.h"

#include <QApplication>
#include <QComboBox>
#include <QDate>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QStackedWidget>
#include <QStackedLayout>
#include <QTableWidget>
#include <QTimer>

namespace ncs {

struct FormalAdminUiTestAccess
{
    static void selectPage(AdminMainWindow &window, int index)
    {
        window.selectPage(index);
    }

    static void applyUsers(UserPage &page, const QVector<User> &users)
    {
        page.applyUsers(users);
    }

    static bool showingEmptyState(const UserPage &page)
    {
        return page.contentStack_->currentWidget() == page.emptyState_;
    }

    static void applyChargers(ChargerPage &page,
                              const QVector<Charger> &chargers)
    {
        page.applyChargers(chargers);
    }

    static void updateChargerActions(ChargerPage &page)
    {
        page.updateActions();
    }

    static void showCreateChargerDialog(ChargerPage &page)
    {
        page.showCreateDialog();
    }
};

}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    ncs::AdminClientFacade facade;
    ncs::AdminLoginWindow login(facade);
    auto *username = login.findChild<QLineEdit *>(
        QStringLiteral("adminUsernameInput"));
    auto *password = login.findChild<QLineEdit *>(
        QStringLiteral("adminPasswordInput"));
    auto *loginButton = login.findChild<QPushButton *>(
        QStringLiteral("adminLoginButton"));
    if (!username || !password || !loginButton
        || username->text() != QStringLiteral("admin")
        || password->echoMode() != QLineEdit::Password) return 1;
    emit facade.requestFailed(QStringLiteral("admin.login"),
        static_cast<int>(ncs::BusinessErrorCode::AdminLocked),
        QStringLiteral("locked"), 30);
    if (username->isEnabled() || password->isEnabled()
        || loginButton->isEnabled()) return 2;

    ncs::Admin admin{1, QStringLiteral("admin"),
                     QStringLiteral("2026-09-04 10:00:00.000")};
    ncs::AdminMainWindow window(admin, facade);
    if (window.minimumWidth() < 1280 || window.minimumHeight() < 800
        || !window.windowTitle().contains(QStringLiteral("admin"))) return 3;
    auto *pages = window.findChild<QStackedWidget *>(QStringLiteral("adminPages"));
    if (!pages || pages->count() != 7) return 4;
    const int initialPage = pages->currentIndex();
    ncs::FormalAdminUiTestAccess::selectPage(window, -1);
    if (pages->currentIndex() != initialPage) return 18;
    ncs::FormalAdminUiTestAccess::selectPage(window, pages->count());
    if (pages->currentIndex() != initialPage) return 19;
    ncs::FormalAdminUiTestAccess::selectPage(window, pages->count() + 1);
    if (pages->currentIndex() != initialPage) return 20;
    ncs::FormalAdminUiTestAccess::selectPage(window, 1);
    if (pages->currentIndex() != 1) return 21;
    const QStringList expected{QStringLiteral("营收分析"), QStringLiteral("电桩状态"),
        QStringLiteral("充电桩管理"), QStringLiteral("充电站管理"),
        QStringLiteral("用户管理"), QStringLiteral("智能预测"), QStringLiteral("日志审计")};
    for (int index = 0; index < expected.size(); ++index) {
        auto *button = window.findChild<QPushButton *>(
            QStringLiteral("adminNavButton%1").arg(index));
        if (!button || button->text() != expected.at(index)) return 5;
    }
    if (!window.findChild<QPushButton *>(QStringLiteral("adminRefreshButton"))
        || !window.findChild<QPushButton *>(QStringLiteral("adminLogoutButton"))
        || !window.findChild<QLabel *>(QStringLiteral("adminDatabaseLabel"))
        || !window.findChild<QLabel *>(QStringLiteral("adminChargerLabel"))
        || !window.findChild<QLabel *>(QStringLiteral("adminClockLabel"))) return 6;

    auto *chargerPage = pages->widget(2);
    auto *chargerSearch = chargerPage ? chargerPage->findChild<QLineEdit *>(
        QStringLiteral("searchField")) : nullptr;
    auto *chargerFilter = chargerPage ? chargerPage->findChild<QPushButton *>(
        QStringLiteral("chargerFilterButton")) : nullptr;
    auto *chargerAdd = chargerPage ? chargerPage->findChild<QPushButton *>(
        QStringLiteral("chargerAddButton")) : nullptr;
    if (!chargerSearch || chargerSearch->isReadOnly()) return 7;
    chargerSearch->setText(QStringLiteral("A-001"));
    if (chargerSearch->text() != QStringLiteral("A-001")
        || chargerSearch->maxLength() <= 0) return 8;
    if (!chargerFilter || !chargerFilter->isEnabled() || !chargerAdd
        || !chargerAdd->isEnabled()) return 9;

    auto *stationPage = pages->widget(3);
    auto *stationSearch = stationPage ? stationPage->findChild<QLineEdit *>(
        QStringLiteral("stationSearchField")) : nullptr;
    auto *stationAdd = stationPage ? stationPage->findChild<QPushButton *>(
        QStringLiteral("stationAddButton")) : nullptr;
    if (!stationSearch || stationSearch->isReadOnly() || !stationAdd
        || !stationAdd->isEnabled()) return 10;

    auto *userPage = pages->widget(4);
    auto *userSearch = userPage ? userPage->findChild<QLineEdit *>(
        QStringLiteral("searchField")) : nullptr;
    auto *userFreeze = userPage ? userPage->findChild<QPushButton *>(
        QStringLiteral("userFreezeButton")) : nullptr;
    auto *userOrders = userPage ? userPage->findChild<QPushButton *>(
        QStringLiteral("userOrdersButton")) : nullptr;
    auto *userUnfreeze = userPage ? userPage->findChild<QPushButton *>(
        QStringLiteral("userUnfreezeButton")) : nullptr;
    auto *userRefresh = userPage ? userPage->findChild<QPushButton *>(
        QStringLiteral("userRefreshButton")) : nullptr;
    if (!userSearch || userSearch->isReadOnly() || !userFreeze || !userUnfreeze
        || !userOrders || !userRefresh || userFreeze->isEnabled()
        || userUnfreeze->isEnabled() || userOrders->isEnabled()) return 11;

    auto *users = qobject_cast<ncs::UserPage *>(userPage);
    if (!users) return 22;
    ncs::FormalAdminUiTestAccess::applyUsers(*users,
                      {{7, QStringLiteral("active"), QStringLiteral("13800000007"),
                        QString(), QString(), 0.0, 1, QStringLiteral("2026-09-06")},
                       {8, QStringLiteral("frozen"), QStringLiteral("13800000008"),
                        QString(), QString(), 0.0, 0, QStringLiteral("2026-09-06")}});
    auto *userTable = users->findChild<QTableWidget *>(QStringLiteral("userTable"));
    if (!userTable || userTable->rowCount() != 2) return 23;
    userTable->selectRow(0);
    if (!userFreeze->isEnabled() || userUnfreeze->isEnabled() || !userOrders->isEnabled()) return 24;
    userTable->selectRow(1);
    if (userFreeze->isEnabled() || !userUnfreeze->isEnabled() || !userOrders->isEnabled()) return 25;
    userTable->clearSelection();
    if (userFreeze->isEnabled() || userUnfreeze->isEnabled() || userOrders->isEnabled()) return 26;
    auto *userState = users->findChild<QLabel *>(QStringLiteral("userStateLabel"));
    userRefresh->click();
    if (!userState || userState->property("state").toString() != QStringLiteral("error")) return 27;
    emit facade.usersReceived({});
    if (!userRefresh->isEnabled()
        || !ncs::FormalAdminUiTestAccess::showingEmptyState(*users)) return 28;

    auto *progress = window.findChild<QProgressBar *>(QStringLiteral("metricTrack"));
    if (!progress || !progress->parentWidget() || progress->value() != 0) return 12;

    auto *revenue = qobject_cast<ncs::RevenuePage *>(pages->widget(0));
    if (!revenue) return 13;
    revenue->applySummary({12.5, 25.0, 100.0});
    if (!revenue->findChild<QLabel *>(QStringLiteral("metricValue"))) return 14;
    ncs::RevenueTrend trend;
    trend.days = 7;
    for (int index = 0; index < 7; ++index) {
        trend.items.append({QDate(2026, 9, 1).addDays(index), index * 1.0, index});
    }
    revenue->applyTrend(trend);
    revenue->applyOrders({{1, QStringLiteral("C-1"), QStringLiteral("测试站"),
                           QStringLiteral("2026-09-01T01:00:00.000Z"),
                           QStringLiteral("2026-09-01T02:00:00.000Z"), 12.5}});
    if (!revenue->findChild<QTableWidget *>(QStringLiteral("recentOrdersTable"))
        || revenue->findChild<QTableWidget *>(QStringLiteral("recentOrdersTable"))->rowCount() != 1) {
        return 15;
    }
    auto *statusPage = qobject_cast<ncs::ChargerStatusPage *>(pages->widget(1));
    if (!statusPage) return 16;
    statusPage->applySummary({10, 5, 3, 2, 50.0, 30.0, 20.0, 80.0});
    auto *health = statusPage->findChild<QLabel *>(QStringLiteral("healthValue"));
    if (!health || health->text() != QStringLiteral("80.0%")) return 17;

    auto *chargers = qobject_cast<ncs::ChargerPage *>(chargerPage);
    if (!chargers) return 29;
    ncs::FormalAdminUiTestAccess::applyChargers(*chargers,
                             {{21, 1, QStringLiteral("IDLE-01"), ncs::ChargerStatus::Idle,
                               -1, 0, 60.0, 0, 0, QStringLiteral("测试站")},
                              {22, 1, QStringLiteral("FAULT-01"), ncs::ChargerStatus::Fault,
                               -1, 0, 60.0, 0, 0, QStringLiteral("测试站")}});
    auto *chargerTable = chargers->findChild<QTableWidget *>(QStringLiteral("chargerTable"));
    auto *deleteButton = chargers->findChild<QPushButton *>(QStringLiteral("chargerDeleteButton"));
    auto *faultButton = chargers->findChild<QPushButton *>(QStringLiteral("chargerFaultButton"));
    auto *recoverButton = chargers->findChild<QPushButton *>(QStringLiteral("chargerRecoverButton"));
    auto *restartButton = chargers->findChild<QPushButton *>(QStringLiteral("chargerRestartButton"));
    if (!chargerTable || !deleteButton || !faultButton || !recoverButton || !restartButton) return 30;
    chargerTable->selectRow(0);
    if (!deleteButton->isEnabled() || !faultButton->isEnabled()
        || recoverButton->isEnabled() || !restartButton->isEnabled()) return 31;
    chargerTable->item(0, 4)->setText(QStringLiteral("任意状态文案"));
    ncs::FormalAdminUiTestAccess::updateChargerActions(*chargers);
    if (!deleteButton->isEnabled() || !faultButton->isEnabled()
        || recoverButton->isEnabled() || !restartButton->isEnabled()) return 34;
    chargerTable->selectRow(1);
    if (!deleteButton->isEnabled() || faultButton->isEnabled()
        || !recoverButton->isEnabled() || !restartButton->isEnabled()) return 32;

    bool dialogChecked = false;
    QTimer::singleShot(0, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog || dialog->objectName() != QStringLiteral("adminModal")
            || dialog->minimumWidth() < 420) return;
        auto *submit = dialog->findChild<QPushButton *>(QStringLiteral("qt_msgbox_buttonbox_button"));
        Q_UNUSED(submit)
        const auto inputs = dialog->findChildren<QLineEdit *>(QStringLiteral("formInput"));
        auto *error = dialog->findChild<QLabel *>(QStringLiteral("formError"));
        if (inputs.size() != 2 || !error) return;
        const auto buttons = dialog->findChildren<QPushButton *>();
        for (auto *button : buttons) {
            if (button->text() == QStringLiteral("新增电桩")) {
                button->click();
                dialogChecked = error->isVisible();
                break;
            }
        }
        dialog->reject();
    });
    ncs::FormalAdminUiTestAccess::showCreateChargerDialog(*chargers);
    if (!dialogChecked) return 33;
    return 0;
}
