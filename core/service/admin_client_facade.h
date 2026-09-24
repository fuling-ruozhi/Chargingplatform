#pragma once

#include "model/admin.h"
#include "model/charger.h"
#include "model/charger_status_summary.h"
#include "model/revenue_stats.h"
#include "model/station.h"
#include "model/user.h"
#include "model/charging_record.h"
#include "model/load_prediction.h"
#include "network/json_protocol.h"
#include "network/network_client.h"
#include "service/request_lifecycle_manager.h"

#include <QHash>
#include <QJsonArray>
#include <QObject>

namespace ncs {

class AsyncLifecycleTestProbe;
class AdminClientFacade : public QObject
{
    Q_OBJECT
public:
    explicit AdminClientFacade(QObject *parent = nullptr);

    void connectToServer(const QString &host = NetworkConfig::defaultHost(),
                         quint16 port = NetworkConfig::DefaultPort);
    void disconnectFromServer();
    bool isConnected() const;
    void login(const QString &username, const QString &password);
    void logout();
    void requestSummary();
    void requestRevenueSummary();
    void requestRevenueTrend(int days);
    void requestRecentOrders();
    void requestChargerStatusSummary();
    void requestChargers(const QString &keyword = QString(), int status = -1, qint64 stationId = -1);
    void requestStations(const QString &keyword = QString());
    void createStation(const ncs::Station &station);
    void updateStation(const ncs::Station &station);
    void deleteStation(qint64 stationId);
    void batchCreateChargers(qint64 stationId, const QString &prefix, int count,
                             int type = 0, double powerKw = 7.0);
    void requestUsers(const QString &keyword = QString());
    void freezeUser(qint64 userId);
    void unfreezeUser(qint64 userId);
    void requestUserOrders(qint64 userId);
    void requestPredictions();
    void runPrediction();
    void createCharger(qint64 stationId, const QString &code, int type,
                       double powerKw);
    void deleteCharger(qint64 chargerId);
    void markChargerFault(qint64 chargerId);
    void recoverCharger(qint64 chargerId);
    void restartCharger(qint64 chargerId);
    void requestLogs(const QString &kind, const QString &keyword = QString(),
                     const QString &type = QString(), const QString &result = QString());

signals:
    void connected();
    void disconnected();
    void loginSucceeded(const ncs::Admin &admin);
    void logoutSucceeded();
    void summaryReceived(const ncs::AdminSummary &summary);
    void revenueSummaryReceived(const ncs::RevenueSummary &summary);
    void revenueTrendReceived(const ncs::RevenueTrend &trend);
    void recentOrdersReceived(const ncs::RecentOrders &orders);
    void chargerStatusReceived(const ncs::ChargerStatusSummary &summary);
    void chargersReceived(const QVector<ncs::Charger> &chargers);
    void stationsReceived(const QVector<ncs::Station> &stations);
    void stationActionSucceeded(const QString &route, qint64 stationId);
    void batchChargersSucceeded(int count);
    void usersReceived(const QVector<ncs::User> &users);
    void userActionSucceeded(const QString &route, qint64 userId);
    void userOrdersReceived(qint64 userId, const QVector<ncs::ChargingRecord> &orders);
    void predictionsReceived(const ncs::PredictionBundle &bundle);
    void predictionRunSucceeded();
    void chargerActionSucceeded(const QString &route, qint64 chargerId);
    void logsReceived(const QString &kind, const QJsonArray &items);
    void requestFailed(const QString &route, int code, const QString &message,
                       int retryAfterSeconds);
    void requestSuppressed(const QString &route);
    void networkError(const QString &message);

private:
    friend class AsyncLifecycleTestProbe;
    void send(const QString &route, QJsonObject data = QJsonObject());
    void sendAuthenticated(const QString &route);
    void sendAnalytics(const QString &route, QJsonObject data = QJsonObject(),
                       int days = 0);
    void failCancelled(const QVector<AsyncRequestContext> &contexts,
                       const QString &message);
    void handle(const JsonResponse &response);
    void handleUserResponse(const QString &route, const JsonResponse &response);
    void handlePredictionResponse(const QString &route, const JsonResponse &response);

    struct PendingRequest {
        QString route;
        int days = 0;
    };

    NetworkClient client_;
    RequestLifecycleManager *requests_ = nullptr;
    QHash<QString, PendingRequest> requestMetadata_;
    QString sessionToken_;
    QString pendingLogKind_;
};

}
