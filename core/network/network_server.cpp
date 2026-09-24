#include "network_server.h"
#include "admin_routes.h"
#include "database/database_manager.h"
#include "json_protocol.h"
#include "business_routes.h"
#include "repository/admin_repository.h"
#include "repository/charger_repository.h"
#include "repository/charge_repository.h"
#include "repository/revenue_repository.h"
#include "repository/prediction_repository.h"
#include "repository/station_repository.h"
#include "repository/user_repository.h"
#include "repository/review_repository.h"
#include "repository/user_preference_repository.h"
#include "repository/favorite_station_repository.h"
#include "service/admin_service.h"
#include "service/charger_service.h"
#include "service/charge_service.h"
#include "service/charge_forecast_service.h"
#include "service/station_service.h"
#include "service/user_service.h"
#include "service/session_manager.h"
#include "service/revenue_service.h"
#include "service/review_service.h"
#include "service/user_preference_service.h"
#include "service/reminder_service.h"
#include "service/prediction_service.h"
#include "util/logger.h"
#include "service/log_service.h"
#include <QAbstractSocket>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
namespace ncs {
namespace {

bool isChargeLifecycleRoute(const QString &route)
{
    return route == QStringLiteral("charge.reserve")
        || route == QStringLiteral("charge.start")
        || route == QStringLiteral("charge.cancel")
        || route == QStringLiteral("charge.active")
        || route == QStringLiteral("charge.settle");
}

}

NetworkServer::NetworkServer(const QString &path, QObject *parent)
    : QObject(parent), databasePath_(path)
{
}

NetworkServer::~NetworkServer() = default;
bool NetworkServer::initializeApplication()
{
    if (databaseManager_) return true;
    auto database = std::make_unique<DatabaseManager>(databasePath_);
    if (!database->initialize()) {
        Logger::error(QStringLiteral("server"),
                      QStringLiteral("Database initialization failed: %1")
                          .arg(database->lastError()));
        emit databaseError(QStringLiteral("数据库初始化失败：%1")
                               .arg(database->lastError()));
        return false;
    }
    Logger::info(QStringLiteral("DB"), QStringLiteral("SQLite opened successfully"));

    auto adminRepository = std::make_unique<AdminRepository>(*database);
    auto logService = std::make_unique<LogService>(database->connection());
    auto revenueRepository = std::make_unique<RevenueRepository>(*database);
    auto revenueService = std::make_unique<RevenueService>(*revenueRepository);
    auto chargerRepository = std::make_unique<ChargerRepository>(*database);
    auto chargerService = std::make_unique<ChargerService>(*chargerRepository);
    auto adminService = std::make_unique<AdminService>(
        *database, *adminRepository, *revenueService, *chargerService);
    adminService->setLogService(*logService);
    auto userRepository = std::make_unique<UserRepository>(*database);
    adminService->setUserRepository(*userRepository);
    auto userService = std::make_unique<UserService>(*database, *userRepository);
    auto sessions = std::make_unique<SessionManager>();
    auto stationRepository = std::make_unique<StationRepository>(*database);
    adminService->setStationRepository(*stationRepository);
    auto chargeRepository = std::make_unique<ChargeRepository>(*database);
    adminService->setChargeRepository(*chargeRepository);
    auto chargeService = std::make_unique<ChargeService>(*database, *chargeRepository);
    auto reviewRepository = std::make_unique<ReviewRepository>(*database);
    auto reviewService = std::make_unique<ReviewService>(*database, *chargeRepository,
                                                         *reviewRepository);
    auto stationService = std::make_unique<StationService>(*stationRepository,
                                                            reviewRepository.get());
    auto userPreferenceRepository = std::make_unique<UserPreferenceRepository>(*database);
    auto favoriteStationRepository = std::make_unique<FavoriteStationRepository>(*database);
    auto userPreferenceService = std::make_unique<UserPreferenceService>(
        *database, *userPreferenceRepository, *favoriteStationRepository);
    auto reminderService = std::make_unique<ReminderService>(
        *database, *userPreferenceRepository, *favoriteStationRepository);
    auto forecastService = std::make_unique<ChargeForecastService>(*chargeRepository,
                                                                    userRepository.get());
    auto predictionRepository = std::make_unique<PredictionRepository>(*database);
    auto predictionService = std::make_unique<PredictionService>(*database, *predictionRepository);
    adminService->setPredictionService(*predictionService);

    BusinessRoutes::registerAll(router_, *userService, *stationService,
                                *chargeService, *sessions, *reviewService,
                                *userPreferenceService, *reminderService,
                                *forecastService);
    AdminRoutes::registerAll(router_, *adminService, *sessions);
    databaseManager_ = std::move(database);
    adminRepository_ = std::move(adminRepository);
    logService_ = std::move(logService);
    adminService_ = std::move(adminService);
    revenueRepository_ = std::move(revenueRepository);
    revenueService_ = std::move(revenueService);
    chargerRepository_ = std::move(chargerRepository);
    chargerService_ = std::move(chargerService);
    userRepository_ = std::move(userRepository);
    userService_ = std::move(userService);
    sessionManager_ = std::move(sessions);
    stationRepository_ = std::move(stationRepository);
    stationService_ = std::move(stationService);
    chargeRepository_ = std::move(chargeRepository);
    chargeService_ = std::move(chargeService);
    reviewRepository_ = std::move(reviewRepository);
    reviewService_ = std::move(reviewService);
    userPreferenceRepository_ = std::move(userPreferenceRepository);
    favoriteStationRepository_ = std::move(favoriteStationRepository);
    userPreferenceService_ = std::move(userPreferenceService);
    reminderService_ = std::move(reminderService);
    forecastService_ = std::move(forecastService);
    predictionRepository_ = std::move(predictionRepository);
    predictionService_ = std::move(predictionService);
    Logger::info(QStringLiteral("server"),
                 QStringLiteral("Application services initialized"));
    return true;
}

void NetworkServer::startServer(const QString &host, quint16 port)
{
    if (!initializeApplication()) return;
    if (!server_) {
        server_ = new QTcpServer(this);
        connect(server_, &QTcpServer::newConnection,
                this, &NetworkServer::acceptPendingConnections);
    }
    if (server_->isListening()) {
        emit started(server_->serverPort());
        return;
    }
    const QHostAddress address(host);
    if (address.isNull()) {
        emit serverError(QStringLiteral("监听地址无效"));
        return;
    }
    if (!server_->listen(address, port)) {
        Logger::error(QStringLiteral("server"), QStringLiteral("Listen failed"));
        emit serverError(QStringLiteral("服务监听失败：%1").arg(server_->errorString()));
        return;
    }
    Logger::info(QStringLiteral("server"), QStringLiteral("TCP server started"));
    emit started(server_->serverPort());
}

void NetworkServer::stopServer()
{
    const auto sockets = codecs_.keys();
    codecs_.clear();
    for (QTcpSocket *socket : sockets) {
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
    if (server_ && server_->isListening()) server_->close();
    Logger::info(QStringLiteral("server"), QStringLiteral("TCP server stopped"));
    emit stopped();
}

void NetworkServer::acceptPendingConnections()
{
    while (server_->hasPendingConnections()) {
        QTcpSocket *socket = server_->nextPendingConnection();
        codecs_.insert(socket, FrameCodec());
        connect(socket, &QTcpSocket::readyRead,
                this, [this, socket] { readFromClient(socket); });
        connect(socket, &QTcpSocket::disconnected,
                this, [this, socket] { removeClient(socket); });
        Logger::info(QStringLiteral("server"), QStringLiteral("Client connected"));
        emit clientConnected(socket->peerAddress().toString());
    }
}

void NetworkServer::readFromClient(QTcpSocket *socket)
{
    auto codec = codecs_.find(socket);
    if (codec == codecs_.end()) return;
    QList<QByteArray> frames;
    QString error;
    if (!codec.value().append(socket->readAll(), &frames, &error)) {
        Logger::warning(QStringLiteral("protocol"), QStringLiteral("Frame rejected"));
        emit protocolError(error);
        if (logService_) logService_->security("INVALID_REQUEST", "MEDIUM", QString(), error);
        sendResponse(socket, JsonProtocol::failure(
            QString(), ProtocolErrorCode::FrameTooLarge, error));
        socket->disconnectFromHost();
        return;
    }
    for (const QByteArray &payload : frames) {
        JsonRequest request;
        ProtocolError protocol;
        if (!JsonProtocol::decodeRequest(payload, &request, &protocol)) {
            Logger::warning(QStringLiteral("protocol"),
                            QStringLiteral("JSON request rejected"));
            sendResponse(socket, JsonProtocol::failure(
                protocol.requestId, protocol.code, protocol.message));
            if (logService_) logService_->security("MALFORMED_JSON", "MEDIUM", QString(), protocol.message);
            continue;
        }
        if (isChargeLifecycleRoute(request.type)) {
            Logger::info(QStringLiteral("network-server"),
                         QStringLiteral("SERVER RECEIVE type=%1 request_id=%2")
                             .arg(request.type, request.requestId));
            Logger::info(QStringLiteral("network-server"),
                         QStringLiteral("SERVER HANDLE type=%1 request_id=%2")
                             .arg(request.type, request.requestId));
        }
        const JsonResponse response = router_.route(request);
        if (!response.success && response.code == static_cast<int>(ProtocolErrorCode::UnknownRequestType)) {
            if (logService_) logService_->security("UNKNOWN_ROUTE", "LOW", QString(), request.type);
        }
        if (isChargeLifecycleRoute(request.type)) {
            Logger::info(QStringLiteral("network-server"),
                         QStringLiteral(
                             "SERVER RESPONSE type=%1 request_id=%2 success=%3 code=%4")
                             .arg(request.type, response.requestId,
                                  response.success ? QStringLiteral("true")
                                                   : QStringLiteral("false"))
                             .arg(response.code));
        }
        sendResponse(socket, response);
    }
}

void NetworkServer::removeClient(QTcpSocket *socket)
{
    codecs_.remove(socket);
    emit clientDisconnected(socket->peerAddress().toString());
    Logger::info(QStringLiteral("server"), QStringLiteral("Client disconnected"));
    socket->deleteLater();
}

void NetworkServer::sendResponse(QTcpSocket *socket, const JsonResponse &response)
{
    const QByteArray frame = FrameCodec::encode(JsonProtocol::encodeResponse(response));
    if (frame.isEmpty() || socket->write(frame) < 0) {
        Logger::error(QStringLiteral("server"), QStringLiteral("Response write failed"));
        emit serverError(QStringLiteral("响应发送失败：%1").arg(socket->errorString()));
    }
}
}
