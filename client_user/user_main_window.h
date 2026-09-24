#pragma once

#include "model/station.h"
#include "service/user_session.h"

#include <QHash>
#include <QMainWindow>
#include <QPointF>
#include <QSet>

class QLabel;
class QPushButton;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QFrame;
class QVBoxLayout;

namespace ncs {

class UserClientFacade;
class ReminderBanner;
class ReminderController;
struct StationDetail;
struct ChargingRecord;

class UserMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    UserMainWindow(const User &user, UserClientFacade &facade, QWidget *parent = nullptr);

signals:
    void logoutRequested();

private:
    void showStations(const QVector<Station> &stations);
    void renderStations();
    void showRecommendations(const QVector<StationRecommendation> &recommendations);
    void refreshStations();
    void requestStationOpen(qint64 stationId);
    void showStationDetail(const StationDetail &detail);
    void setState(const QString &message, bool error = false);
    void openProfile();
    void openPreference();
    void updateUser(const User &user);
    void recoverActiveOrder(bool hasActive, const ChargingRecord &record);
    void locateAddress();
    double originLongitude() const;
    double originLatitude() const;

    UserSession session_;
    UserClientFacade &facade_;
    QLabel *stateLabel_ = nullptr;
    QLabel *welcomeLabel_ = nullptr;
    QLabel *balanceLabel_ = nullptr;
    QComboBox *regionBox_ = nullptr;
    QComboBox *sortBox_ = nullptr;
    QDoubleSpinBox *socInput_ = nullptr;
    QLineEdit *addressEdit_ = nullptr;
    QFrame *recommendationPanel_ = nullptr;
    QVBoxLayout *recommendationLayout_ = nullptr;
    QVBoxLayout *stationLayout_ = nullptr;
    QVector<Station> stations_;
    QHash<qint64, QLabel *> availabilityLabels_;
    QHash<qint64, QPushButton *> stationButtons_;
    QSet<qint64> availabilityPending_;
    QPointF locatedOrigin_;
    qint64 pendingOpenStationId_ = 0;
    bool hasLocatedOrigin_ = false;
    bool initialRecoveryPending_ = true;
    ReminderController *reminderController_ = nullptr;
    ReminderBanner *reminderBanner_ = nullptr;
};

}
