cmake_minimum_required(VERSION 3.16)
project(hms_colada CXX)

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
    find_package(eclipse-paho-mqtt-c CONFIG REQUIRED)
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
    GIT_TAG        v1.6.6
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
        eclipse-paho-mqtt-c::paho-mqtt3as
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

# Executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    include
)

# Link libraries
if(MSVC)
    target_link_libraries(${PROJECT_NAME}
        JsonCpp::JsonCpp
        PahoMqttCpp::paho-mqttpp3
        eclipse-paho-mqtt-c::paho-mqtt3as
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
