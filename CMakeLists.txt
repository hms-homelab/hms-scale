cmake_minimum_required(VERSION 3.16)

# The VERSION file is the single source of truth for the version string, so the
# binary cannot drift from the tag the way it had (VERSION said 1.1.0 while
# every log line still announced 1.0.0). CMAKE_CONFIGURE_DEPENDS matters here:
# without it CMake reads the file once at configure time and a later bump does
# not retrigger configure, so the build quietly keeps stamping the old value.
file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSION" _version_raw)
string(STRIP "${_version_raw}" HMS_COLADA_VERSION)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_CURRENT_LIST_DIR}/VERSION")
if(NOT HMS_COLADA_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must be MAJOR.MINOR.PATCH, got '${HMS_COLADA_VERSION}'")
endif()

project(hms_colada VERSION ${HMS_COLADA_VERSION} LANGUAGES CXX)

# C++20 on MSVC (needed for designated initializers in hms-shared/db),
# C++17 on Linux (system pqxx is built with C++17 — ABI mismatch if we use C++20)
if(MSVC)
    set(CMAKE_CXX_STANDARD 20)
else()
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform-aware compiler flags
if(MSVC)
    add_compile_options(/W4 /O2 /utf-8)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -pthread")
endif()

# Build options
option(BUILD_WITH_WEB "Build with Drogon web UI server" ON)
option(BUILD_TESTS "Build unit tests" ON)

# PostgreSQL — always required
add_definitions(-DWITH_POSTGRESQL)

# Find required packages
if(BUILD_WITH_WEB)
    find_package(Drogon CONFIG REQUIRED)
    add_definitions(-DBUILD_WITH_WEB)
endif()

if(MSVC)
    find_package(jsoncpp CONFIG REQUIRED)
else()
    find_package(jsoncpp REQUIRED)
endif()

find_package(OpenSSL REQUIRED)

# nlohmann_json
if(CMAKE_CROSSCOMPILING AND NOT WIN32)
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
    set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "/usr/include"
    )
else()
    find_package(nlohmann_json CONFIG REQUIRED)
endif()

# Paho MQTT
if(MSVC)
    find_package(PahoMqttCpp CONFIG REQUIRED)
else()
    find_library(PAHO_MQTTPP3_LIB paho-mqttpp3)
    find_library(PAHO_MQTT3AS_LIB paho-mqtt3as)
endif()

# PostgreSQL (pqxx + libpq)
if(MSVC)
    find_package(libpqxx CONFIG REQUIRED)
    find_package(PostgreSQL REQUIRED)
else()
    find_library(PQXX_LIB pqxx)
    find_library(PQ_LIB pq)
    # libpq-fe.h lives in /usr/include/postgresql on Debian
    find_path(PQ_INCLUDE_DIR libpq-fe.h PATHS /usr/include/postgresql)
    if(PQ_INCLUDE_DIR)
        include_directories(${PQ_INCLUDE_DIR})
    endif()
endif()

# spdlog
find_package(spdlog REQUIRED)

# ── External libraries (via FetchContent) ────────────────────────────────────
include(FetchContent)

# hms-shared modules
FetchContent_Declare(hms_shared
    GIT_REPOSITORY https://github.com/hms-homelab/hms-shared.git
    GIT_TAG        v1.6.18
)
FetchContent_Populate(hms_shared)

# MQTT module (spdlog + paho)
add_library(hms_mqtt STATIC
    ${hms_shared_SOURCE_DIR}/mqtt/src/mqtt_client.cpp
)
target_include_directories(hms_mqtt PUBLIC
    ${hms_shared_SOURCE_DIR}/mqtt/include
)
if(MSVC)
    target_compile_definitions(hms_mqtt PUBLIC PAHO_MQTTPP_IMPORTS)
    target_link_libraries(hms_mqtt PUBLIC
        spdlog::spdlog
        PahoMqttCpp::paho-mqttpp3
    )
else()
    target_link_libraries(hms_mqtt PUBLIC
        spdlog::spdlog
        ${PAHO_MQTTPP3_LIB}
        ${PAHO_MQTT3AS_LIB}
    )
endif()

# DB module (DbPool connection pool)
add_library(hms_db STATIC
    ${hms_shared_SOURCE_DIR}/db/src/db_pool.cpp
)
target_include_directories(hms_db PUBLIC
    ${hms_shared_SOURCE_DIR}/db/include
)
if(MSVC)
    target_link_libraries(hms_db PUBLIC libpqxx::pqxx PostgreSQL::PostgreSQL)
else()
    target_link_libraries(hms_db PUBLIC ${PQXX_LIB} ${PQ_LIB})
endif()

# BLE support (optional, requires libsdbus-c++)
option(BUILD_WITH_BLE "Build with direct BLE scale connectivity via BlueZ" OFF)
if(BUILD_WITH_BLE)
    find_package(sdbus-c++ REQUIRED)
    message(STATUS "BLE: Enabled (sdbus-c++ ${sdbus-c++_VERSION})")
else()
    message(STATUS "BLE: Disabled (pass -DBUILD_WITH_BLE=ON to enable)")
endif()

# mDNS support (optional, requires libavahi-client). Defaults ON so that the
# hms-scale.local:8889 the ESP firmware ships with actually resolves, but Avahi
# is Linux-only and this must never be the reason someone's build fails: if the
# headers are absent the feature quietly turns itself off. Pass
# -DBUILD_WITH_MDNS=OFF to opt out deliberately.
option(BUILD_WITH_MDNS "Publish the service over mDNS/zeroconf via Avahi" ON)
if(BUILD_WITH_MDNS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_library(AVAHI_CLIENT_LIB avahi-client)
    find_library(AVAHI_COMMON_LIB avahi-common)
    find_path(AVAHI_INCLUDE_DIR avahi-client/publish.h)
    if(AVAHI_CLIENT_LIB AND AVAHI_COMMON_LIB AND AVAHI_INCLUDE_DIR)
        message(STATUS "mDNS: Enabled (${AVAHI_CLIENT_LIB})")
    else()
        message(STATUS "mDNS: Disabled (libavahi-client-dev not found)")
        set(BUILD_WITH_MDNS OFF)
    endif()
elseif(BUILD_WITH_MDNS)
    message(STATUS "mDNS: Disabled (Avahi is Linux-only, this is ${CMAKE_SYSTEM_NAME})")
    set(BUILD_WITH_MDNS OFF)
else()
    message(STATUS "mDNS: Disabled (BUILD_WITH_MDNS=OFF)")
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/database/PostgresScaleDatabase.cpp
    src/calculation/BodyCompositionCalculator.cpp
    src/ml/DecisionTree.cpp
    src/ml/RandomForest.cpp
    src/ml/ScaleFeatureEngine.cpp
    src/identification/HybridEngine.cpp
    src/services/MLTrainingService.cpp
    src/mqtt/DiscoveryPublisher.cpp
    src/mqtt/ScaleSubscriber.cpp
    src/analytics/HabitAnalyzer.cpp
    src/controllers/ColadaController.cpp
)

if(BUILD_WITH_BLE)
    list(APPEND SOURCES src/ble/BleScaleClient.cpp)
endif()

if(BUILD_WITH_MDNS)
    list(APPEND SOURCES src/services/MdnsPublisher.cpp)
endif()

# Executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    HMS_COLADA_VERSION="${HMS_COLADA_VERSION}")

# Link libraries
if(MSVC)
    target_link_libraries(${PROJECT_NAME}
        JsonCpp::JsonCpp
        PahoMqttCpp::paho-mqttpp3
        OpenSSL::SSL
        OpenSSL::Crypto
        libpqxx::pqxx
        PostgreSQL::PostgreSQL
        hms_mqtt
        hms_db
        spdlog::spdlog
        nlohmann_json::nlohmann_json
    )
else()
    target_link_libraries(${PROJECT_NAME}
        jsoncpp_lib
        ${PAHO_MQTTPP3_LIB}
        ${PAHO_MQTT3AS_LIB}
        OpenSSL::SSL
        OpenSSL::Crypto
        ${PQXX_LIB}
        ${PQ_LIB}
        hms_mqtt
        hms_db
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        stdc++fs
        pthread
    )
endif()

if(BUILD_WITH_WEB)
    target_link_libraries(${PROJECT_NAME} Drogon::Drogon)
endif()

if(BUILD_WITH_MDNS)
    target_include_directories(${PROJECT_NAME} PRIVATE ${AVAHI_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${AVAHI_CLIENT_LIB} ${AVAHI_COMMON_LIB})
    target_compile_definitions(${PROJECT_NAME} PUBLIC WITH_MDNS=1)
endif()

if(BUILD_WITH_BLE)
    target_link_libraries(${PROJECT_NAME} SDBusCpp::sdbus-c++)
    target_compile_definitions(${PROJECT_NAME} PUBLIC WITH_BLE=1)
    # Pass major version so source can handle 1.x vs 2.x API differences
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        SDBUS_CXX_MAJOR_VERSION=${sdbus-c++_VERSION_MAJOR})
endif()

# Install target
if(WIN32)
    install(TARGETS ${PROJECT_NAME} DESTINATION bin)
else()
    install(TARGETS ${PROJECT_NAME} DESTINATION /usr/local/bin)
endif()

# Testing support
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Print configuration
message(STATUS "HMS-Colada Build Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
if(MSVC)
    message(STATUS "  Paho MQTT: vcpkg (static)")
    message(STATUS "  PostgreSQL: vcpkg")
else()
    message(STATUS "  Paho MQTT: ${PAHO_MQTTPP3_LIB}")
    message(STATUS "  PostgreSQL: ${PQXX_LIB}")
endif()
message(STATUS "  DB Pool: hms_db (from hms-shared)")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
