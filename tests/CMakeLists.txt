# HMS-Colada Unit Tests
cmake_minimum_required(VERSION 3.16)

# Find Google Test
find_package(GTest REQUIRED)
include(GoogleTest)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${GTEST_INCLUDE_DIRS}
)
if(NOT MSVC)
    include_directories(/usr/local/include)
endif()

# Project source files (exclude main.cpp)
set(PROJECT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/database/PostgresScaleDatabase.cpp
    ${CMAKE_SOURCE_DIR}/src/calculation/BodyCompositionCalculator.cpp
    ${CMAKE_SOURCE_DIR}/src/ml/DecisionTree.cpp
    ${CMAKE_SOURCE_DIR}/src/ml/RandomForest.cpp
    ${CMAKE_SOURCE_DIR}/src/ml/ScaleFeatureEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/identification/HybridEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/services/MLTrainingService.cpp
    ${CMAKE_SOURCE_DIR}/src/mqtt/DiscoveryPublisher.cpp
    ${CMAKE_SOURCE_DIR}/src/mqtt/ScaleSubscriber.cpp
    ${CMAKE_SOURCE_DIR}/src/analytics/HabitAnalyzer.cpp
    ${CMAKE_SOURCE_DIR}/src/controllers/ColadaController.cpp
)

# Test sources
set(TEST_SOURCES
    test_database.cpp
    test_bia.cpp
    test_ml.cpp
    test_identification.cpp
    test_mqtt.cpp
    test_habits.cpp
)

# Create test executable
add_executable(run_tests
    ${TEST_SOURCES}
    ${PROJECT_SOURCES}
)

# Link libraries
if(MSVC)
    target_link_libraries(run_tests
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        JsonCpp::JsonCpp
        libpqxx::pqxx
        PostgreSQL::PostgreSQL
        hms_db
        hms_mqtt
        spdlog::spdlog
        nlohmann_json::nlohmann_json
    )
else()
    target_link_libraries(run_tests
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        jsoncpp_lib
        ${PQXX_LIB}
        ${PQ_LIB}
        hms_db
        hms_mqtt
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        stdc++fs
        pthread
    )
endif()

if(BUILD_WITH_WEB)
    target_link_libraries(run_tests Drogon::Drogon)
endif()

# Discover tests
gtest_discover_tests(run_tests DISCOVERY_MODE PRE_TEST)

message(STATUS "HMS-Colada Tests Configuration:")
message(STATUS "  Test sources: ${TEST_SOURCES}")
