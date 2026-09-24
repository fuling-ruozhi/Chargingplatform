#include "station_detail_dialog.h"

#include "service/user_client_facade.h"
#include "smart_charging_dialog.h"

namespace ncs {

void StationDetailDialog::openSmartCharging(const Charger &charger)
{
    SmartChargingDialog dialog(detail_.station, charger, userBalance_, this);
    if (dialog.exec() != QDialog::Accepted || !dialog.hasSelectedPlan()) return;
    pendingSmartPlan_ = dialog.selectedPlan();
    pendingSmartInput_ = dialog.selectedInput();
    hasPendingSmartPlan_ = true;
    beginAction(QStringLiteral("charge.reserve"), charger.id,
                QStringLiteral("正在通过正式流程预约，请稍候…"));
    facade_.reserveCharge(userId_, charger.id);
}

}
