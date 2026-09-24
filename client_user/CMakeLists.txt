find_package(Qt6 QUIET COMPONENTS WebEngineWidgets)
find_package(Qt6 QUIET COMPONENTS DBus)
find_package(Qt6 QUIET COMPONENTS Network)

add_executable(ncs_user
    main.cpp
    login_window.cpp login_window.h
    user_main_window.cpp user_main_window.h
    user_main_window_stations.cpp
    user_main_window_render.cpp
    user_main_window_recommendations.cpp
    reminder_controller.cpp reminder_controller.h
    reminder_banner.cpp reminder_banner.h
    user_profile_dialog.cpp user_profile_dialog.h
    user_preference_dialog.cpp user_preference_dialog.h
    order_history_dialog.cpp order_history_dialog.h
    station_sorting.cpp station_sorting.h
    charging_dialog.cpp charging_dialog.h
    review_dialog.cpp review_dialog.h
    forecast_card.cpp forecast_card.h
    avatar_image_processor.cpp avatar_image_processor.h
    avatar_source_dialog.cpp avatar_source_dialog.h
    camera_capture_dialog.cpp camera_capture_dialog.h
    station_detail_dialog.cpp station_detail_dialog.h
    station_detail_actions.cpp
    station_detail_smart_charging.cpp
    charging_dialog_states.cpp
    navigation_launcher.cpp navigation_launcher.h
    charging_dialog_smart.cpp
    smart_charging_dialog.cpp smart_charging_dialog.h
    user_messages.h
    vehicle_profile_dialog.cpp vehicle_profile_dialog.h
    resources/client_user.qrc
)
target_link_libraries(ncs_user PRIVATE
    ncs_core ncs_client_common Qt6::Widgets
    Qt6::Multimedia Qt6::MultimediaWidgets)
if(Qt6DBus_FOUND)
    target_compile_definitions(ncs_user PRIVATE NCS_HAS_DBUS)
    target_link_libraries(ncs_user PRIVATE Qt6::DBus)
endif()
if(Qt6WebEngineWidgets_FOUND)
    target_compile_definitions(ncs_user PRIVATE NCS_HAS_WEBENGINE)
    target_sources(ncs_user PRIVATE
        map_view_dialog.cpp map_view_dialog.h
        map_route_dialog.cpp map_route_dialog.h
        map_route_planner.cpp map_route_planner.h)
    target_link_libraries(ncs_user PRIVATE
        Qt6::WebEngineWidgets Qt6::Network)
endif()
