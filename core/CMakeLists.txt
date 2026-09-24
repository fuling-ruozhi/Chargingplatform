add_library(ncs_core STATIC
    config/network_config.h config/charge_config.cpp config/charge_config.h
    database/database_manager.cpp database/database_manager.h
    database/schema_initializer.cpp database/schema_initializer.h
    database/database_migration_manager.cpp database/database_migration_manager.h
    database/logging_schema_migration.cpp database/logging_schema_migration.h
    database/formal_schema_migration.cpp database/formal_schema_migration.h
    database/formal_schema_validation.cpp database/formal_schema_validation.h
    database/preference_schema_migration.cpp database/preference_schema_migration.h
    database/user_schema_migration.cpp database/user_schema_migration.h
    database/seed_data.cpp database/seed_data.h
    database/seed_data_history.cpp database/seed_data_history.h
    model/business_error.h model/service_result.h model/admin.h model/user.h model/station.h
    model/charger.cpp model/charger.h model/station_detail.h
    model/charging_record.h model/review.h model/user_preference.h model/load_prediction.h
    repository/admin_repository.cpp repository/admin_repository.h
    repository/revenue_repository.cpp repository/revenue_repository.h
    repository/charger_repository.cpp repository/charger_repository.h
    repository/user_repository.cpp repository/user_repository.h
    repository/station_repository.cpp repository/station_repository.h
    repository/charge_repository.cpp repository/charge_settlement_repository.cpp
    repository/charge_history_repository.cpp
    repository/review_repository.cpp repository/review_repository.h
    repository/user_preference_repository.cpp repository/user_preference_repository.h
    repository/favorite_station_repository.cpp repository/favorite_station_repository.h
    repository/prediction_repository.cpp repository/prediction_repository.h
    repository/charge_repository.h
    service/prediction_service.cpp service/prediction_service.h
    service/admin_service.cpp service/admin_service.h
    service/log_service.cpp service/log_service.h
    service/revenue_service.cpp service/revenue_service.h
    service/charger_service.cpp service/charger_service.h
    service/user_service.cpp service/user_service.h
    service/auth_rate_limiter.cpp service/auth_rate_limiter.h
    service/session_manager.cpp service/session_manager.h
    service/request_lifecycle_manager.cpp service/request_lifecycle_manager.h
    service/station_service.cpp service/station_service.h
    service/station_recommendation.cpp service/station_recommendation.h
    service/station_rating_insight.cpp service/station_rating_insight.h
    service/charge_service.cpp service/charge_reservation_service.cpp
    service/charge_settlement_service.cpp service/charge_history_service.cpp
    service/charge_service.h
    service/review_service.cpp service/review_service.h
    service/user_preference_service.cpp service/user_preference_service.h
    service/reminder_service.cpp service/reminder_service.h
    service/charge_forecast_service.cpp service/charge_forecast_service.h
    service/user_client_facade_forecast.cpp
    service/admin_client_facade_analytics.cpp
    service/admin_client_facade.cpp service/admin_client_facade.h
    service/admin_client_facade_charger.cpp
    service/admin_client_facade_prediction.cpp
    service/admin_client_facade_user.cpp
    service/user_client_facade.cpp service/user_client_facade.h service/user_session.h
    service/user_client_facade_legacy.cpp
    service/user_client_facade_vehicle.cpp
    service/user_client_facade_recommendations.cpp
    service/user_client_facade_json.h
    util/password_hasher.cpp util/password_hasher.h
    util/charge_calculator.cpp util/charge_calculator.h
    util/smart_charging_planner.cpp util/smart_charging_planner.h
    util/date_time_storage.cpp util/date_time_storage.h util/money.h
    util/geo_distance.cpp util/geo_distance.h util/time_window.cpp util/time_window.h
    util/navigation_url.cpp util/navigation_url.h
    util/route_decoder.cpp util/route_decoder.h
    util/logger.cpp util/logger.h
    network/admin_routes.cpp network/admin_routes.h
    network/admin_route_validation.cpp network/admin_route_validation.h
    network/admin_routes_logging.cpp
    network/admin_routes_prediction.cpp
    network/admin_route_json.cpp network/admin_route_json.h
    network/business_routes.cpp network/business_routes.h
    network/business_routes_helpers.cpp network/business_routes_helpers.h
    network/business_routes_recommendation.cpp
    network/business_routes_forecast.cpp
    network/business_route_json.cpp network/business_route_json.h
    network/preference_routes.cpp network/preference_routes.h
    network/frame_codec.cpp network/frame_codec.h
    network/json_protocol.cpp network/json_protocol.h
    network/network_client.cpp network/network_client.h
    network/network_server.cpp network/network_server.h
    network/review_json.cpp network/review_json.h
    network/network_server_host.cpp network/network_server_host.h
    network/request_router.cpp network/request_router.h
    version.cpp version.h
    ../db/database.qrc
)
target_link_libraries(ncs_core PUBLIC Qt6::Core Qt6::Sql Qt6::Network)
target_include_directories(ncs_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
