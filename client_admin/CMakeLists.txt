add_executable(ncs_admin
    main.cpp
    admin_login_window.cpp admin_login_window.h
    admin_main_window.cpp admin_main_window.h
    ui_icons.cpp icon_assets.h
    pages/dashboard_ui.cpp pages/dashboard_ui.h
    pages/revenue_page.cpp pages/revenue_page.h
    pages/charger_status_page.cpp pages/charger_status_page.h
    pages/charger_page.cpp pages/charger_page.h
    pages/station_page.cpp pages/station_page.h
    pages/station_page_dialogs.cpp
    pages/user_page.cpp pages/user_page.h
    pages/predict_page.cpp pages/predict_page.h
    pages/predict_page_chart.cpp
    pages/log_page.cpp pages/log_page.h
    resources/client_admin.qrc
)
target_link_libraries(ncs_admin PRIVATE ncs_core ncs_client_common Qt6::Widgets Qt6::Charts)
