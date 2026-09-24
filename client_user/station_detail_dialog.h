#pragma once

#include "model/charging_record.h"
#include "model/station_detail.h"
#include "util/smart_charging_planner.h"

#include <QDialog>
#include <QPointer>
#include <QVector>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace ncs {

class UserClientFacade;
class ConfirmActionDialog;

class StationDetailDialog : public QDialog
{
    Q_OBJECT

public:
    StationDetailDialog(const StationDetail &detail, qint64 userId,
                        UserClientFacade &facade, QWidget *parent = nullptr,
                        double originLongitude = 0.0, double originLatitude = 0.0,
                        bool hasOrigin = false);
    StationDetailDialog(const StationDetail &detail, qint64 userId,
                        UserClientFacade &facade, QWidget *parent,
                        double originLongitude, double originLatitude,
                        bool hasOrigin, double userBalance);

signals:
    void rechargeRequested();

private:
    void requestCurrentState(bool refreshDetail);
    void finishRefreshIfReady();
    void renderChargers();
    void renderRatingSummary();
    void beginAction(const QString &route, qint64 chargerId, const QString &message);
    void resetAction(const QString &message);
    void openCharging(const ChargingRecord &record);
    void openSmartCharging(const Charger &charger);
    void openNavigation();
    void updateFavoriteButton(bool favorite);
#ifdef NCS_HAS_WEBENGINE
    void openMapView();
#endif

    StationDetail detail_;
    qint64 userId_ = 0;
    UserClientFacade &facade_;
    ChargingRecord activeRecord_;
    bool hasActive_ = false;
    bool awaitingDetail_ = false;
    bool awaitingActive_ = false;
    int refreshGeneration_ = 0;
    QString pendingRoute_;
    qint64 pendingChargerId_ = 0;
    QLabel *requestStateLabel_ = nullptr;
    QLabel *ratingLabel_ = nullptr;
    QPushButton *rechargeButton_ = nullptr;
    QPushButton *favoriteButton_ = nullptr;
    QVBoxLayout *chargerLayout_ = nullptr;
    QVector<QPushButton *> actionButtons_;
    double originLongitude_ = 0.0;
    double originLatitude_ = 0.0;
    bool hasOrigin_ = false;
    bool favorite_ = false;
    bool favoriteRequestPending_ = false;
    double userBalance_ = 0.0;
    bool hasPendingSmartPlan_ = false;
    SmartChargingPlan pendingSmartPlan_;
    SmartChargingInput pendingSmartInput_;
    QPointer<ConfirmActionDialog> confirmation_;
};

}
