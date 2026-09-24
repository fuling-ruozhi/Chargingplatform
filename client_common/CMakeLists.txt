add_library(ncs_client_common STATIC
    confirm_action_dialog.cpp confirm_action_dialog.h)
target_link_libraries(ncs_client_common PUBLIC Qt6::Widgets)
target_include_directories(ncs_client_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
