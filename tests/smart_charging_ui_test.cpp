#include "charging_dialog.h"
#include "service/user_client_facade.h"
#include "smart_charging_dialog.h"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

namespace {

bool containsLabel(QWidget &widget, const QString &text)
{
    for (QLabel *label : widget.findChildren<QLabel *>()) {
        if (label->text().contains(text)) return true;
    }
    return false;
}

bool containsButton(QWidget &widget, const QString &text)
{
    for (QPushButton *button : widget.findChildren<QPushButton *>()) {
        if (button->text().contains(text)) return true;
    }
    return false;
}

}  // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    ncs::Station station;
    station.id = 1;
    station.name = QStringLiteral("智慧测试站");
    station.price = 1.2;
    ncs::Charger charger;
    charger.id = 1;
    charger.stationId = 1;
    charger.code = QStringLiteral("NCS-SMART-01");
    charger.powerKw = 120.0;

    ncs::SmartChargingDialog dialog(station, charger, 50.0);
    dialog.show();
    QApplication::processEvents();
    auto *current = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("currentSocInput"));
    auto *target = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("targetSocInput"));
    auto *capacity = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("batteryCapacityInput"));
    auto *reserve = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("reserveBalanceInput"));
    auto *minutes = dialog.findChild<QSpinBox *>(QStringLiteral("availableMinutesInput"));
    auto *generate = dialog.findChild<QPushButton *>(QStringLiteral("generateSmartPlanButton"));
    auto *apply = dialog.findChild<QPushButton *>(QStringLiteral("applySmartPlanButton"));
    if (!current || !target || !capacity || !reserve || !minutes || !generate || !apply) return 1;
    if (!dialog.findChild<QPushButton *>(QStringLiteral("fastestPlanCard"))
        || !dialog.findChild<QPushButton *>(QStringLiteral("balancedPlanCard"))
        || !dialog.findChild<QPushButton *>(QStringLiteral("economyPlanCard"))) return 2;
    if (!dialog.hasSelectedPlan() || !apply->isEnabled()
        || !containsButton(dialog, QStringLiteral("推荐"))
        || !containsLabel(dialog, QStringLiteral("现实预计"))) return 3;

    reserve->setValue(49.0);
    generate->click();
    QApplication::processEvents();
    if (!containsLabel(dialog, QStringLiteral("余额约束"))) return 4;

    reserve->setValue(10.0);
    minutes->setValue(1);
    generate->click();
    QApplication::processEvents();
    if (!containsLabel(dialog, QStringLiteral("时间约束"))) return 5;

    current->setValue(90.0);
    target->setValue(80.0);
    generate->click();
    QApplication::processEvents();
    if (!containsLabel(dialog, QStringLiteral("目标电量必须")) || apply->isEnabled()) return 6;

    current->setValue(30.0);
    target->setValue(80.0);
    minutes->setValue(60);
    generate->click();
    const ncs::SmartChargingPlan plan = dialog.selectedPlan();
    const ncs::SmartChargingInput input = dialog.selectedInput();
    if (!dialog.hasSelectedPlan()) return 7;

    ncs::UserClientFacade facade;
    ncs::ChargingRecord record;
    record.id = 1;
    record.userId = 1;
    record.chargerId = charger.id;
    record.chargerCode = charger.code;
    record.stationName = station.name;
    record.price = station.price;
    record.powerKw = charger.powerKw;
    record.reservedAt = QStringLiteral("2026-09-04 10:00:00.000");
    record.expireAt = QStringLiteral("2099-09-04 10:15:00.000");
    record.status = ncs::ChargingOrderStatus::Reserved;
    ncs::ChargingDialog charging(record, charger.code, station.price, facade,
                                 nullptr, &plan, &input);
    charging.show();
    QApplication::processEvents();
    if (!containsLabel(charging, QStringLiteral("智慧目标"))) return 8;
    return 0;
}
