#include "charging_dialog.h"
#include "service/user_client_facade.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>

namespace {

bool contains(QWidget &widget, const QString &text)
{
    for (QLabel *label : widget.findChildren<QLabel *>()) {
        if (label->text().contains(text)) return true;
    }
    return false;
}

}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    ncs::UserClientFacade facade;
    ncs::ChargingRecord active;
    active.id = 8;
    active.orderNo = QStringLiteral("NCS-RECEIPT-8");
    active.userId = 1;
    active.chargerId = 2;
    active.stationName = QStringLiteral("测试电站");
    active.chargerCode = QStringLiteral("NCS-01-02");
    active.startTime = QStringLiteral("2026-09-03 10:00:00.000");
    active.price = 1.2;
    active.powerKw = 7.0;
    active.status = ncs::ChargingOrderStatus::Charging;
    ncs::ChargingDialog dialog(active, active.chargerCode, active.price, facade);
    dialog.show();

    ncs::ChargingRecord completed = active;
    completed.status = ncs::ChargingOrderStatus::Completed;
    completed.endTime = QStringLiteral("2026-09-03 10:30:00.000");
    completed.durationSeconds = 1800;
    completed.energy = 3.5;
    completed.cost = 4.2;
    completed.finalSoc = 25.8;
    completed.debtAmount = 1.2;
    completed.balanceAfter = 0.0;
    emit facade.chargeStopped(completed);
    QApplication::processEvents();

    auto *receipt = dialog.findChild<QLabel *>(QStringLiteral("receiptDetails"));
    if (!receipt || !receipt->isVisibleTo(&dialog)
        || !contains(dialog, QStringLiteral("NCS-RECEIPT-8"))
        || !contains(dialog, QStringLiteral("2026-09-03 10:30:00"))
        || !contains(dialog, QStringLiteral("电价：1.20 元/kWh"))
        || !contains(dialog, QStringLiteral("欠费：1.20 元"))
        || !contains(dialog, QStringLiteral("扣款后余额：0.00 元"))) return 1;
    return 0;
}
