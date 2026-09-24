add_executable(ncs_network_protocol_test network_protocol_test.cpp)
target_link_libraries(ncs_network_protocol_test PRIVATE ncs_core)
add_test(NAME ncs_network_protocol COMMAND ncs_network_protocol_test)
add_executable(ncs_logging_service_test logging_service_test.cpp)
target_link_libraries(ncs_logging_service_test PRIVATE ncs_core)
add_test(NAME ncs_logging_service COMMAND ncs_logging_service_test)
add_executable(ncs_network_integration_test network_integration_test.cpp)
target_link_libraries(ncs_network_integration_test PRIVATE ncs_core)
add_test(NAME ncs_network_integration COMMAND ncs_network_integration_test)
add_executable(ncs_phase31_user_test phase31_user_test.cpp)
target_link_libraries(ncs_phase31_user_test PRIVATE ncs_core)
add_test(NAME ncs_phase31_user COMMAND ncs_phase31_user_test)
add_executable(ncs_phase31_tcp_test phase31_tcp_test.cpp)
target_link_libraries(ncs_phase31_tcp_test PRIVATE ncs_core)
add_test(NAME ncs_phase31_tcp COMMAND ncs_phase31_tcp_test)
add_executable(ncs_phase32_station_test phase32_station_test.cpp)
target_link_libraries(ncs_phase32_station_test PRIVATE ncs_core)
add_test(NAME ncs_phase32_station COMMAND ncs_phase32_station_test)
add_executable(ncs_phase32_tcp_test phase32_tcp_test.cpp)
target_link_libraries(ncs_phase32_tcp_test PRIVATE ncs_core)
add_test(NAME ncs_phase32_tcp COMMAND ncs_phase32_tcp_test)
add_executable(ncs_phase33_detail_test phase33_detail_test.cpp)
target_link_libraries(ncs_phase33_detail_test PRIVATE ncs_core)
add_test(NAME ncs_phase33_detail COMMAND ncs_phase33_detail_test)
add_executable(ncs_phase33_tcp_test phase33_tcp_test.cpp)
target_link_libraries(ncs_phase33_tcp_test PRIVATE ncs_core)
add_test(NAME ncs_phase33_tcp COMMAND ncs_phase33_tcp_test)
add_executable(ncs_phase34_charge_test phase34_charge_test.cpp)
target_link_libraries(ncs_phase34_charge_test PRIVATE ncs_core)
add_test(NAME ncs_phase34_charge COMMAND ncs_phase34_charge_test)
add_executable(ncs_phase34_tcp_test phase34_tcp_test.cpp)
target_link_libraries(ncs_phase34_tcp_test PRIVATE ncs_core)
add_test(NAME ncs_phase34_tcp COMMAND ncs_phase34_tcp_test)
add_executable(test_user_login_roundtrip test_user_login_roundtrip.cpp)
target_link_libraries(test_user_login_roundtrip PRIVATE ncs_core)
add_test(NAME test_user_login_roundtrip COMMAND test_user_login_roundtrip)
add_executable(test_user_schema_migration test_user_schema_migration.cpp)
target_link_libraries(test_user_schema_migration PRIVATE ncs_core)
add_test(NAME test_user_schema_migration COMMAND test_user_schema_migration)
add_executable(test_phase2_to_phase3_migration test_phase2_to_phase3_migration.cpp)
target_link_libraries(test_phase2_to_phase3_migration PRIVATE ncs_core)
add_test(NAME test_phase2_to_phase3_migration COMMAND test_phase2_to_phase3_migration)
add_executable(ncs_phase4_charging_flow_test phase4_charging_flow_test.cpp)
target_link_libraries(ncs_phase4_charging_flow_test PRIVATE ncs_core)
add_test(NAME ncs_phase4_charging_flow COMMAND ncs_phase4_charging_flow_test)

add_executable(ncs_phase5_c1_order_flow_test phase5_c1_order_flow_test.cpp)
target_link_libraries(ncs_phase5_c1_order_flow_test PRIVATE ncs_core)
add_test(NAME ncs_phase5_c1_order_flow COMMAND ncs_phase5_c1_order_flow_test)

add_executable(ncs_phase5_c1_tcp_test phase5_c1_tcp_test.cpp)
target_link_libraries(ncs_phase5_c1_tcp_test PRIVATE ncs_core)
add_test(NAME ncs_phase5_c1_tcp COMMAND ncs_phase5_c1_tcp_test)

add_executable(ncs_phase5_c1_migration_test phase5_c1_migration_test.cpp)
target_link_libraries(ncs_phase5_c1_migration_test PRIVATE ncs_core)
add_test(NAME ncs_phase5_c1_migration COMMAND ncs_phase5_c1_migration_test)

add_executable(ncs_formal_schema_migration_test formal_schema_migration_test.cpp)
target_link_libraries(ncs_formal_schema_migration_test PRIVATE ncs_core)
add_test(NAME ncs_formal_schema_migration COMMAND ncs_formal_schema_migration_test)

add_executable(ncs_review_backend_test review_backend_test.cpp)
target_link_libraries(ncs_review_backend_test PRIVATE ncs_core)
add_test(NAME ncs_review_backend COMMAND ncs_review_backend_test)

add_executable(ncs_formal_user_auth_test formal_user_auth_test.cpp)
target_link_libraries(ncs_formal_user_auth_test PRIVATE ncs_core)
add_test(NAME ncs_formal_user_auth COMMAND ncs_formal_user_auth_test)
add_executable(ncs_auth_rate_limit_test auth_rate_limit_test.cpp)
target_link_libraries(ncs_auth_rate_limit_test PRIVATE ncs_core)
add_test(NAME ncs_auth_rate_limit COMMAND ncs_auth_rate_limit_test)
add_executable(ncs_station_repository_test station_repository_test.cpp)
target_link_libraries(ncs_station_repository_test PRIVATE ncs_core)
add_test(NAME ncs_station_repository COMMAND ncs_station_repository_test)

add_executable(ncs_formal_user_auth_tcp_test formal_user_auth_tcp_test.cpp)
target_link_libraries(ncs_formal_user_auth_tcp_test PRIVATE ncs_core)
add_test(NAME ncs_formal_user_auth_tcp COMMAND ncs_formal_user_auth_tcp_test)

add_executable(ncs_formal_order_source_test formal_order_source_test.cpp)
target_link_libraries(ncs_formal_order_source_test PRIVATE ncs_core)
add_test(NAME ncs_formal_order_source COMMAND ncs_formal_order_source_test)

add_executable(ncs_formal_order_auth_tcp_test formal_order_auth_tcp_test.cpp)
target_link_libraries(ncs_formal_order_auth_tcp_test PRIVATE ncs_core)
add_test(NAME ncs_formal_order_auth_tcp COMMAND ncs_formal_order_auth_tcp_test)

add_executable(ncs_formal_reservation_rules_test formal_reservation_rules_test.cpp)
target_link_libraries(ncs_formal_reservation_rules_test PRIVATE ncs_core)
add_test(NAME ncs_formal_reservation_rules COMMAND ncs_formal_reservation_rules_test)

add_executable(ncs_formal_billing_test formal_billing_test.cpp)
target_link_libraries(ncs_formal_billing_test PRIVATE ncs_core)
add_test(NAME ncs_formal_billing COMMAND ncs_formal_billing_test)

add_executable(ncs_formal_recovery_test formal_recovery_test.cpp)
target_link_libraries(ncs_formal_recovery_test PRIVATE ncs_core)
add_test(NAME ncs_formal_recovery COMMAND ncs_formal_recovery_test)

add_executable(ncs_formal_settlement_test formal_settlement_test.cpp)
target_link_libraries(ncs_formal_settlement_test PRIVATE ncs_core)
add_test(NAME ncs_formal_settlement COMMAND ncs_formal_settlement_test)

add_executable(ncs_formal_settlement_ui_test
    formal_settlement_ui_test.cpp
    ../client_user/charging_dialog.cpp
    ../client_user/charging_dialog_states.cpp
    ../client_user/charging_dialog_smart.cpp
    ../client_user/review_dialog.cpp)
target_include_directories(ncs_formal_settlement_ui_test PRIVATE ../client_user)
target_link_libraries(ncs_formal_settlement_ui_test PRIVATE ncs_core ncs_client_common Qt6::Widgets)
add_test(NAME ncs_formal_settlement_ui COMMAND ncs_formal_settlement_ui_test)
set_tests_properties(ncs_formal_settlement_ui PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

add_executable(ncs_review_ui_test
    review_ui_test.cpp
    ../client_user/review_dialog.cpp
    ../client_user/station_detail_dialog.cpp
    ../client_user/station_detail_actions.cpp
    ../client_user/station_detail_smart_charging.cpp
    ../client_user/smart_charging_dialog.cpp
    ../client_user/navigation_launcher.cpp
    ../client_user/charging_dialog.cpp
    ../client_user/charging_dialog_states.cpp
    ../client_user/charging_dialog_smart.cpp)
target_include_directories(ncs_review_ui_test PRIVATE ../client_user)
target_link_libraries(ncs_review_ui_test PRIVATE ncs_core ncs_client_common Qt6::Widgets)
add_test(NAME ncs_review_ui COMMAND ncs_review_ui_test)
set_tests_properties(ncs_review_ui PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

add_executable(ncs_review_recommendation_test
    review_recommendation_test.cpp
    ../client_user/station_sorting.cpp)
target_include_directories(ncs_review_recommendation_test PRIVATE ../client_user)
target_link_libraries(ncs_review_recommendation_test PRIVATE ncs_core Qt6::Widgets)
add_test(NAME ncs_review_recommendation COMMAND ncs_review_recommendation_test)

add_executable(ncs_formal_order_history_test formal_order_history_test.cpp)
target_link_libraries(ncs_formal_order_history_test PRIVATE ncs_core)
add_test(NAME ncs_formal_order_history COMMAND ncs_formal_order_history_test)

add_executable(ncs_formal_station_geo_test formal_station_geo_test.cpp)
target_link_libraries(ncs_formal_station_geo_test PRIVATE ncs_core)
add_test(NAME ncs_formal_station_geo COMMAND ncs_formal_station_geo_test)

add_executable(ncs_route_decoder_test formal_route_decoder_test.cpp)
target_link_libraries(ncs_route_decoder_test PRIVATE ncs_core)
add_test(NAME ncs_route_decoder COMMAND ncs_route_decoder_test)

add_executable(ncs_station_recommendation_test station_recommendation_test.cpp)
target_link_libraries(ncs_station_recommendation_test PRIVATE ncs_core)
add_test(NAME ncs_station_recommendation COMMAND ncs_station_recommendation_test)

add_executable(ncs_formal_admin_auth_test formal_admin_auth_test.cpp)
target_link_libraries(ncs_formal_admin_auth_test PRIVATE ncs_core)
add_test(NAME ncs_formal_admin_auth COMMAND ncs_formal_admin_auth_test)

add_executable(ncs_formal_admin_tcp_test formal_admin_tcp_test.cpp)
target_link_libraries(ncs_formal_admin_tcp_test PRIVATE ncs_core)
add_test(NAME ncs_formal_admin_tcp COMMAND ncs_formal_admin_tcp_test)

add_executable(ncs_formal_admin_analytics_test formal_admin_analytics_test.cpp)
target_link_libraries(ncs_formal_admin_analytics_test PRIVATE ncs_core)
add_test(NAME ncs_formal_admin_analytics COMMAND ncs_formal_admin_analytics_test)

add_executable(ncs_formal_admin_analytics_facade_test formal_admin_analytics_facade_test.cpp)
target_link_libraries(ncs_formal_admin_analytics_facade_test PRIVATE ncs_core)
add_test(NAME ncs_formal_admin_analytics_facade COMMAND ncs_formal_admin_analytics_facade_test)

add_executable(ncs_formal_admin_charger_test formal_admin_charger_test.cpp)
target_link_libraries(ncs_formal_admin_charger_test PRIVATE ncs_core)
add_test(NAME ncs_formal_admin_charger COMMAND ncs_formal_admin_charger_test)

add_executable(ncs_formal_admin_station_test formal_admin_station_test.cpp)
target_link_libraries(ncs_formal_admin_station_test PRIVATE ncs_core)
add_test(NAME ncs_formal_admin_station COMMAND ncs_formal_admin_station_test)

add_executable(ncs_formal_admin_user_test formal_admin_user_test.cpp)
target_link_libraries(ncs_formal_admin_user_test PRIVATE ncs_core)
add_test(NAME ncs_formal_admin_user COMMAND ncs_formal_admin_user_test)

add_executable(ncs_formal_admin_ui_test
    formal_admin_ui_test.cpp
    ../client_admin/admin_login_window.cpp
    ../client_admin/admin_main_window.cpp
    ../client_admin/ui_icons.cpp
    ../client_admin/pages/dashboard_ui.cpp
    ../client_admin/pages/revenue_page.cpp
    ../client_admin/pages/charger_status_page.cpp
    ../client_admin/pages/charger_page.cpp
    ../client_admin/pages/station_page.cpp
    ../client_admin/pages/station_page_dialogs.cpp
    ../client_admin/pages/user_page.cpp
    ../client_admin/pages/predict_page.cpp
    ../client_admin/pages/predict_page_chart.cpp
    ../client_admin/pages/log_page.cpp)
target_include_directories(ncs_formal_admin_ui_test PRIVATE ../client_admin)
target_link_libraries(ncs_formal_admin_ui_test PRIVATE ncs_core ncs_client_common Qt6::Widgets Qt6::Charts)
add_test(NAME ncs_formal_admin_ui COMMAND ncs_formal_admin_ui_test)
set_tests_properties(ncs_formal_admin_ui PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

add_executable(ncs_formal_charge_forecast_test formal_charge_forecast_test.cpp)
target_link_libraries(ncs_formal_charge_forecast_test PRIVATE ncs_core)
add_test(NAME ncs_formal_charge_forecast COMMAND ncs_formal_charge_forecast_test)

add_executable(ncs_formal_nfr_test formal_nfr_test.cpp)
target_link_libraries(ncs_formal_nfr_test PRIVATE ncs_core)
target_compile_definitions(ncs_formal_nfr_test PRIVATE
    NCS_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME ncs_formal_nfr COMMAND ncs_formal_nfr_test)

add_executable(ncs_reminder_preference_test reminder_preference_test.cpp)
target_link_libraries(ncs_reminder_preference_test PRIVATE ncs_core)
add_test(NAME ncs_reminder_preference COMMAND ncs_reminder_preference_test)

add_executable(ncs_formal_seed_data_test formal_seed_data_test.cpp)
target_link_libraries(ncs_formal_seed_data_test PRIVATE ncs_core)
add_test(NAME ncs_formal_seed_data COMMAND ncs_formal_seed_data_test)

add_executable(ncs_formal_prediction_async_test formal_prediction_async_test.cpp)
target_link_libraries(ncs_formal_prediction_async_test PRIVATE ncs_core)
add_test(NAME ncs_formal_prediction_async COMMAND ncs_formal_prediction_async_test)
set_tests_properties(ncs_formal_prediction_async PROPERTIES TIMEOUT 60)

add_executable(ncs_formal_recovery_ui_test
    formal_recovery_ui_test.cpp
    ../client_user/user_main_window.cpp
    ../client_user/user_main_window_render.cpp
    ../client_user/forecast_card.cpp
    ../client_user/user_main_window_stations.cpp
    ../client_user/user_main_window_recommendations.cpp
    ../client_user/user_profile_dialog.cpp
    ../client_user/avatar_image_processor.cpp
    ../client_user/avatar_source_dialog.cpp
    ../client_user/camera_capture_dialog.cpp
    ../client_user/order_history_dialog.cpp
    ../client_user/station_detail_dialog.cpp
    ../client_user/charging_dialog.cpp
    ../client_user/review_dialog.cpp
    ../client_user/station_sorting.cpp
    ../client_user/reminder_controller.cpp
    ../client_user/reminder_banner.cpp
    ../client_user/user_preference_dialog.cpp
    ../client_user/station_detail_actions.cpp
    ../client_user/station_detail_smart_charging.cpp
    ../client_user/smart_charging_dialog.cpp
    ../client_user/charging_dialog_states.cpp
    ../client_user/charging_dialog_smart.cpp
    ../client_user/navigation_launcher.cpp
    ../client_user/vehicle_profile_dialog.cpp)
target_include_directories(ncs_formal_recovery_ui_test PRIVATE ../client_user)
target_link_libraries(ncs_formal_recovery_ui_test PRIVATE
    ncs_core ncs_client_common Qt6::Widgets
    Qt6::Multimedia Qt6::MultimediaWidgets)
add_test(NAME ncs_formal_recovery_ui COMMAND ncs_formal_recovery_ui_test)
set_tests_properties(ncs_formal_recovery_ui PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

add_executable(ncs_formal_user_ui_test
    formal_user_ui_test.cpp
    ../client_user/login_window.cpp
    ../client_user/user_profile_dialog.cpp)
target_sources(ncs_formal_user_ui_test PRIVATE
    ../client_user/forecast_card.cpp
    ../client_user/avatar_image_processor.cpp
    ../client_user/avatar_source_dialog.cpp
    ../client_user/camera_capture_dialog.cpp
    ../client_user/vehicle_profile_dialog.cpp
    ../client_user/order_history_dialog.cpp
    ../client_user/review_dialog.cpp)
target_include_directories(ncs_formal_user_ui_test PRIVATE ../client_user)
target_link_libraries(ncs_formal_user_ui_test PRIVATE
    ncs_core ncs_client_common Qt6::Widgets
    Qt6::Multimedia Qt6::MultimediaWidgets)
add_test(NAME ncs_formal_user_ui COMMAND ncs_formal_user_ui_test)
set_tests_properties(ncs_formal_user_ui PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

add_executable(ncs_user_preference_ui_test
    user_preference_ui_test.cpp
    ../client_user/user_preference_dialog.cpp
    ../client_user/reminder_controller.cpp
    ../client_user/station_detail_dialog.cpp
    ../client_user/station_detail_actions.cpp
    ../client_user/station_detail_smart_charging.cpp
    ../client_user/smart_charging_dialog.cpp
    ../client_user/navigation_launcher.cpp
    ../client_user/charging_dialog.cpp
    ../client_user/charging_dialog_states.cpp
    ../client_user/charging_dialog_smart.cpp
    ../client_user/review_dialog.cpp)
target_include_directories(ncs_user_preference_ui_test PRIVATE ../client_user)
target_link_libraries(ncs_user_preference_ui_test PRIVATE ncs_core ncs_client_common Qt6::Widgets)
add_test(NAME ncs_user_preference_ui COMMAND ncs_user_preference_ui_test)
set_tests_properties(ncs_user_preference_ui PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

add_executable(ncs_phase5_c1_charge_entry_test phase5_c1_charge_entry_test.cpp)
target_link_libraries(ncs_phase5_c1_charge_entry_test PRIVATE ncs_core)
add_test(NAME ncs_phase5_c1_charge_entry COMMAND ncs_phase5_c1_charge_entry_test)

add_executable(ncs_phase5_c1_charge_entry_ui_test
    phase5_c1_charge_entry_ui_test.cpp
    ../client_user/station_detail_dialog.cpp
    ../client_user/charging_dialog.cpp
    ../client_user/station_detail_actions.cpp
    ../client_user/station_detail_smart_charging.cpp
    ../client_user/smart_charging_dialog.cpp
    ../client_user/charging_dialog_states.cpp
    ../client_user/charging_dialog_smart.cpp
    ../client_user/navigation_launcher.cpp
    ../client_user/review_dialog.cpp)
target_include_directories(ncs_phase5_c1_charge_entry_ui_test PRIVATE ../client_user)
target_link_libraries(ncs_phase5_c1_charge_entry_ui_test PRIVATE ncs_core ncs_client_common Qt6::Widgets)
add_test(NAME ncs_phase5_c1_charge_entry_ui COMMAND ncs_phase5_c1_charge_entry_ui_test)
set_tests_properties(ncs_phase5_c1_charge_entry_ui PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

add_executable(ncs_vehicle_profile_test formal_vehicle_profile_test.cpp)
target_link_libraries(ncs_vehicle_profile_test PRIVATE ncs_core)
add_test(NAME ncs_vehicle_profile COMMAND ncs_vehicle_profile_test)

set_tests_properties(
    ncs_phase31_tcp
    ncs_phase34_tcp
    ncs_phase5_c1_tcp
    ncs_formal_user_auth_tcp
    ncs_formal_order_auth_tcp
    ncs_formal_recovery
    ncs_formal_order_history
    ncs_formal_admin_tcp
    ncs_phase5_c1_charge_entry
    PROPERTIES ENVIRONMENT NCS_DEMO_OTP_ECHO=1)

add_executable(ncs_smart_charging_planner_test smart_charging_planner_test.cpp)
target_link_libraries(ncs_smart_charging_planner_test PRIVATE ncs_core)
add_test(NAME ncs_smart_charging_planner COMMAND ncs_smart_charging_planner_test)

add_executable(ncs_smart_charging_ui_test
    smart_charging_ui_test.cpp
    ../client_user/smart_charging_dialog.cpp
    ../client_user/charging_dialog.cpp
    ../client_user/charging_dialog_states.cpp
    ../client_user/charging_dialog_smart.cpp
    ../client_user/review_dialog.cpp)
target_include_directories(ncs_smart_charging_ui_test PRIVATE ../client_user)
target_link_libraries(ncs_smart_charging_ui_test PRIVATE ncs_core ncs_client_common Qt6::Widgets)
add_test(NAME ncs_smart_charging_ui COMMAND ncs_smart_charging_ui_test)
set_tests_properties(ncs_smart_charging_ui PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

add_executable(ncs_charge_request_dispatch_regression_test
    charge_request_dispatch_regression_test.cpp)
target_link_libraries(ncs_charge_request_dispatch_regression_test PRIVATE ncs_core)
add_test(NAME ncs_charge_request_dispatch_regression
    COMMAND ncs_charge_request_dispatch_regression_test)

add_executable(ncs_async_lifecycle_test async_lifecycle_test.cpp)
target_link_libraries(ncs_async_lifecycle_test PRIVATE ncs_core)
add_test(NAME ncs_async_lifecycle COMMAND ncs_async_lifecycle_test)

add_executable(ncs_confirmation_lifecycle_test confirmation_lifecycle_test.cpp)
target_link_libraries(ncs_confirmation_lifecycle_test
    PRIVATE ncs_client_common Qt6::Widgets)
add_test(NAME ncs_confirmation_lifecycle COMMAND ncs_confirmation_lifecycle_test)
set_tests_properties(ncs_confirmation_lifecycle
    PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

add_executable(ncs_session_cycle_test session_cycle_test.cpp)
target_link_libraries(ncs_session_cycle_test PRIVATE ncs_core)
add_test(NAME ncs_session_cycle COMMAND ncs_session_cycle_test)

add_executable(ncs_pending_request_test pending_request_test.cpp)
target_link_libraries(ncs_pending_request_test PRIVATE ncs_core)
add_test(NAME ncs_pending_request COMMAND ncs_pending_request_test)

add_executable(ncs_avatar_image_processor_test
    avatar_image_processor_test.cpp
    ../client_user/avatar_image_processor.cpp)
target_include_directories(ncs_avatar_image_processor_test PRIVATE ../client_user)
target_link_libraries(ncs_avatar_image_processor_test PRIVATE Qt6::Widgets)
add_test(NAME ncs_avatar_image_processor COMMAND ncs_avatar_image_processor_test)

add_executable(ncs_avatar_source_ui_test
    avatar_source_ui_test.cpp
    ../client_user/user_profile_dialog.cpp
    ../client_user/forecast_card.cpp
    ../client_user/avatar_image_processor.cpp
    ../client_user/avatar_source_dialog.cpp
    ../client_user/camera_capture_dialog.cpp
    ../client_user/order_history_dialog.cpp
    ../client_user/review_dialog.cpp
    ../client_user/vehicle_profile_dialog.cpp)
target_include_directories(ncs_avatar_source_ui_test PRIVATE ../client_user)
target_link_libraries(ncs_avatar_source_ui_test PRIVATE
    ncs_core ncs_client_common Qt6::Widgets
    Qt6::Multimedia Qt6::MultimediaWidgets)
add_test(NAME ncs_avatar_source_ui COMMAND ncs_avatar_source_ui_test)
set_tests_properties(ncs_avatar_source_ui
    PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
