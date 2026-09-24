#pragma once

#include "frame_codec.h"
#include "request_router.h"

#include <QHash>
#include <QObject>
#include <memory>

class QTcpServer;
class QTcpSocket;

namespace ncs {

class DatabaseManager;
class AdminRepository;
class AdminService;
class ChargerRepository;
class ChargerService;
class ChargeRepository;
class ChargeService;
class ReviewRepository;
class ReviewService;
class StationRepository;
class StationService;
class UserRepository;
class UserService;
class ChargeForecastService;
class SessionManager;
class RevenueRepository;
class RevenueService;
class FavoriteStationRepository;
class UserPreferenceRepository;
class UserPreferenceService;
class ReminderService;
class PredictionRepository;
class PredictionService;
class LogService;

class NetworkServer : public QObject
{
    Q_OBJECT
public:
    explicit NetworkServer(const QString &databasePath = QString(), QObject *parent = nullptr);
    ~NetworkServer() override;

public slots:
    void startServer(const QString &host, quint16 port);
    void stopServer();

signals:
    void started(quint16 port);
    void stopped();
    void serverError(const QString &message);
    void databaseError(const QString &message);
    void protocolError(const QString &message);
    void clientConnected(const QString &peer);
    void clientDisconnected(const QString &peer);

private slots:
    void acceptPendingConnections();

private:
    void readFromClient(QTcpSocket *socket);
    void removeClient(QTcpSocket *socket);
    void sendResponse(QTcpSocket *socket, const JsonResponse &response);
    bool initializeApplication();

    QString databasePath_;
    QTcpServer *server_ = nullptr;
    QHash<QTcpSocket *, FrameCodec> codecs_;
    RequestRouter router_;
    std::unique_ptr<DatabaseManager> databaseManager_;
    std::unique_ptr<AdminRepository> adminRepository_;
    std::unique_ptr<AdminService> adminService_;
    std::unique_ptr<LogService> logService_;
    std::unique_ptr<RevenueRepository> revenueRepository_;
    std::unique_ptr<RevenueService> revenueService_;
    std::unique_ptr<ChargerRepository> chargerRepository_;
    std::unique_ptr<ChargerService> chargerService_;
    std::unique_ptr<ChargeRepository> chargeRepository_;
    std::unique_ptr<ChargeService> chargeService_;
    std::unique_ptr<ReviewRepository> reviewRepository_;
    std::unique_ptr<ReviewService> reviewService_;
    std::unique_ptr<ChargeForecastService> forecastService_;
    std::unique_ptr<StationRepository> stationRepository_;
    std::unique_ptr<StationService> stationService_;
    std::unique_ptr<UserRepository> userRepository_;
    std::unique_ptr<UserService> userService_;
    std::unique_ptr<PredictionRepository> predictionRepository_;
    std::unique_ptr<PredictionService> predictionService_;
    std::unique_ptr<SessionManager> sessionManager_;
    std::unique_ptr<UserPreferenceRepository> userPreferenceRepository_;
    std::unique_ptr<FavoriteStationRepository> favoriteStationRepository_;
    std::unique_ptr<UserPreferenceService> userPreferenceService_;
    std::unique_ptr<ReminderService> reminderService_;
};

}  // namespace ncs
