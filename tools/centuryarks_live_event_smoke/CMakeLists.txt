cmake_minimum_required(VERSION 3.16)

project(centuryarks_live_event_smoke LANGUAGES CXX)

find_package(
    MetavisionSDK 5.2.0 EXACT
    CONFIG REQUIRED COMPONENTS stream
)

add_executable(centuryarks_live_event_smoke main.cpp)

set_target_properties(centuryarks_live_event_smoke PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_link_libraries(centuryarks_live_event_smoke
    PRIVATE
        MetavisionSDK::stream
)

if(APPLE)
    # This ignored validation binary is not installed or distributed.
    set_target_properties(centuryarks_live_event_smoke PROPERTIES
        BUILD_RPATH "$<TARGET_FILE_DIR:MetavisionSDK::stream>"
        INSTALL_RPATH ""
    )
endif()
