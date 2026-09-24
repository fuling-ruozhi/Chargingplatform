#include "charging_dialog.h"

#include "util/charge_calculator.h"

#include <QLabel>
#include <QProgressBar>

namespace ncs {

ChargingDialog::ChargingDialog(const ChargingRecord &record,
                               const QString &chargerCode, double stationPrice,
                               UserClientFacade &facade, QWidget *parent)
    : ChargingDialog(record, chargerCode, stationPrice, facade, parent, nullptr, nullptr)
{}

void ChargingDialog::updateSmartSoc(const ChargeMetrics &metrics)
{
    double displaySoc = metrics.soc;
    if (hasSmartPlan_) displaySoc = qBound(0.0,
        smartInput_.currentSocPercent
            + metrics.energyKwh / smartInput_.batteryCapacityKwh * 100.0, 100.0);
    socLabel_->setText(hasSmartPlan_
        ? QStringLiteral("%1% / 目标 %2%").arg(displaySoc, 0, 'f', 1)
              .arg(smartPlan_.recommendedTargetSoc, 0, 'f', 1)
        : QStringLiteral("%1%").arg(displaySoc, 0, 'f', 1));
    socProgress_->setValue(qRound(displaySoc * 10.0));
    if (hasSmartPlan_ && displaySoc + 0.05 >= smartPlan_.recommendedTargetSoc) {
        stateLabel_->setText(QStringLiteral("已达到智慧方案目标，可以结束充电"));
    }
}

}
