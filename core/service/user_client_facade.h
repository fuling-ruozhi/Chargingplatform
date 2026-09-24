#pragma once
#include "model/station.h"
#include "model/station_detail.h"
#include "model/charge_forecast.h"
#include "model/charging_record.h"
#include "model/user.h"
#include "model/review.h"
#include "model/user_preference.h"
#include "model/vehicle_profile.h"
#include "network/network_client.h"
#include "service/request_lifecycle_manager.h"
#include <QObject>
#include <QVector>
namespace ncs {
class UserClientFacadeTestProbe;
class AsyncLifecycleTestProbe;
class UserClientFacade : public QObject
{
    Q_OBJECT
public:
    explicit UserClientFacade(QObject *parent = nullptr);
    ~UserClientFacade() override;
    void connectToServer(const QString &host = NetworkConfig::defaultHost(),
                         quint16 port = NetworkConfig::DefaultPort);
    void disconnectFromServer();
    bool isConnected() const;
    void requestOtp(const QString &phone);
    void login(const QString &phone, const QString &code);
    void requestProfile();
    void requestPreference();
    void updatePreference(const ncs::UserPreference &preference);
    void requestFavoriteStations();
    void addFavoriteStation(qint64 stationId);
    void removeFavoriteStation(qint64 stationId);
    void checkReminders();
    void requestChargeForecast();
    void updateNickname(const QString &nickname);
    void updateAvatar(const QString &avatarPath);
    void recharge(double amount);
    void logout();
    void requestVehicleProfile();
    void updateVehicleProfile(const VehicleProfile &profile);

    // Deprecated compatibility methods; the formal client UI never calls these.
    void registerUser(const QString &, const QString &);
    void legacyLogin(const QString &, const QString &);
    void requestStations();
    void requestStations(double longitude, double latitude);
    void requestStationRecommendations(double longitude, double latitude,
                                       double currentSocPercent);
    void requestStationDetail(qint64 stationId);
    void requestStationDetail(qint64 stationId, double longitude, double latitude);
    void reserveCharge(qint64 userId, qint64 chargerId);
    void startCharge(qint64 userId, qint64 chargerId, qint64 recordId = 0);
    void stopCharge(qint64 recordId);
    void cancelReservation(qint64 userId, qint64 recordId);
    void requestActiveCharge(qint64 userId);
    void requestOrders(int page = 1, int pageSize = 20);
    void requestOrderDetail(qint64 orderId);
    void submitReview(qint64 orderId, int environmentScore, int queueScore,
                      int equipmentScore, int parkingScore);
    void requestReview(qint64 orderId);
signals:
    void connected();
    void disconnected();
    void sessionInvalid();
    void registerSucceeded(const ncs::User &);
    void otpReceived(const QString &displayCode, int cooldownSeconds,
                     int expiresSeconds);
    void loginSucceeded(const ncs::User &);
    void profileReceived(const ncs::User &);
    void chargeForecastReceived(const ncs::ChargeForecast &);
    void profileUpdated(const ncs::User &);
    void preferenceReceived(const ncs::UserPreference &);
    void preferenceUpdated(const ncs::UserPreference &);
    void favoriteStationsReceived(const QVector<qint64> &);
    void favoriteStationChanged(qint64 stationId, bool added);
    void remindersReceived(const QVector<ncs::ReminderMatch> &);
    void rechargeSucceeded(const ncs::RechargeResult &);
    void logoutSucceeded();
    void vehicleProfileReceived(const ncs::VehicleProfile &profile);
    void vehicleProfileUpdated(const ncs::VehicleProfile &profile);
    void stationsReceived(const QVector<ncs::Station> &);
    void stationRecommendationsReceived(
        const QVector<ncs::StationRecommendation> &);
    void stationDetailReceived(const ncs::StationDetail &);
    void reservationCreated(const ncs::ChargingRecord &);
    void chargeStarted(const ncs::ChargingRecord &);
    void chargeStopped(const ncs::ChargingRecord &);
    void reservationCancelled(const ncs::ChargingRecord &);
    void activeChargeReceived(bool hasActive, const ncs::ChargingRecord &record);
    void ordersReceived(const QVector<ncs::ChargingRecord> &records);
    void orderDetailReceived(const ncs::ChargingRecord &record);
    void reviewSubmitted(const ncs::Review &review);
    void reviewReceived(bool hasReview, const ncs::Review &review);
    void requestFailed(const QString &, int, const QString &);
    void requestSuppressed(const QString &route);
    void networkError(const QString &);
private:
    friend class UserClientFacadeTestProbe;
    friend class AsyncLifecycleTestProbe;
    void send(const QString &, const QString &, const QString &);
    void send(const QString &, const QJsonObject &);
    void sendAuthenticated(const QString &, QJsonObject data = QJsonObject());
    void expireRequest(const QString &requestId);
    void failCancelled(const QVector<AsyncRequestContext> &contexts,
                       const QString &message);
    bool handleStationRecommendationResponse(const JsonResponse &response,
                                             const QString &route);
    void handle(const JsonResponse &);
    void handleVehicleProfile(const QString &route, const JsonResponse &response);
    void handleForecastResponse(const JsonResponse &response);
    NetworkClient client_;
    RequestLifecycleManager *requests_ = nullptr;
    QString sessionToken_;
};
}
