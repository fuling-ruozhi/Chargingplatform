#pragma once

#include "model/charging_record.h"
#include "util/smart_charging_planner.h"

#include <QDialog>
#include <QPointer>

class QFrame;
class QLabel;
class QPushButton;
class QProgressBar;
class QTimer;

namespace ncs {

class UserClientFacade;
class ConfirmActionDialog;
struct ChargeMetrics;

class ChargingDialog : public QDialog
{
    Q_OBJECT

public:
    ChargingDialog(const ChargingRecord &record, const QString &chargerCode,
                   double stationPrice, UserClientFacade &facade,
                   QWidget *parent = nullptr);
    ChargingDialog(const ChargingRecord &record, const QString &chargerCode,
                   double stationPrice, UserClientFacade &facade, QWidget *parent,
                   const SmartChargingPlan *smartPlan,
                   const SmartChargingInput *smartInput);

private:
    void refresh();
    void showReservation();
    void showCharging();
    void openReviewDialog();
    void updateSmartSoc(const ChargeMetrics &metrics);
    void setRequestState(const QString &message);
    void resetRequestState(const QString &message);

    ChargingRecord record_;
    QString chargerCode_;
    double stationPrice_ = 0.0;
    UserClientFacade &facade_;
    QTimer *timer_ = nullptr;
    QFrame *metricsFrame_ = nullptr;
    QFrame *receiptFrame_ = nullptr;
    QLabel *stationLabel_ = nullptr;
    QLabel *timeLabel_ = nullptr;
    QLabel *timeCaption_ = nullptr;
    QLabel *chargerLabel_ = nullptr;
    QLabel *startLabel_ = nullptr;
    QLabel *startCaption_ = nullptr;
    QLabel *energyLabel_ = nullptr;
    QLabel *costCaption_ = nullptr;
    QLabel *costLabel_ = nullptr;
    QLabel *powerLabel_ = nullptr;
    QLabel *socLabel_ = nullptr;
    QProgressBar *socProgress_ = nullptr;
    QLabel *stateLabel_ = nullptr;
    QLabel *receiptLabel_ = nullptr;
    QPushButton *actionButton_ = nullptr;
    QPushButton *cancelButton_ = nullptr;
    QString pendingRoute_;
    bool expiryCheckRequested_ = false;
    bool hasSmartPlan_ = false;
    SmartChargingPlan smartPlan_;
    SmartChargingInput smartInput_;
    QPointer<ConfirmActionDialog> confirmation_;
};

}
