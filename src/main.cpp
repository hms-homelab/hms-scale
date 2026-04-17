#include <csignal>
#include <filesystem>
#include <memory>

#include <spdlog/spdlog.h>

#include "utils/AppConfig.h"
#include "database/PostgresScaleDatabase.h"
#include "calculation/BodyCompositionCalculator.h"
#include "identification/HybridEngine.h"
#include "services/MLTrainingService.h"
#include "analytics/HabitAnalyzer.h"
#include "mqtt/ScaleSubscriber.h"
#include "mqtt/DiscoveryPublisher.h"

#ifdef BUILD_WITH_WEB
#include <drogon/drogon.h>
#include "controllers/ColadaController.h"
#endif

#include <mqtt_client.h>

static std::atomic<bool> g_running{true};

static void signalHandler(int sig) {
    spdlog::info("Signal {} received, shutting down...", sig);
    g_running = false;
#ifdef BUILD_WITH_WEB
    drogon::app().quit();
#endif
}

int main() {
    spdlog::info("hms-colada v1.0.0 starting...");

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // ── Config ──────────────────────────────────────────────────────────
    std::string config_dir = AppConfig::dataDir();
    std::string config_path = config_dir + "/config.json";
    std::filesystem::create_directories(config_dir);

    AppConfig config;
    if (AppConfig::load(config_path, config)) {
        spdlog::info("Loaded config from {}", config_path);
    } else {
        spdlog::info("No config found, using defaults");
        config.save(config_path);
    }
    config.applyEnvFallbacks();

    // ── Database ────────────────────────────────────────────────────────
    auto db = std::make_unique<PostgresScaleDatabase>(config.connectionString());
    if (!db->connect()) {
        spdlog::error("Failed to connect to PostgreSQL");
        return 1;
    }
    spdlog::info("Connected to PostgreSQL: {}", config.database.name);

    // ── Services ────────────────────────────────────────────────────────
    auto calculator = std::make_unique<hms_colada::BodyCompositionCalculator>();
    auto habit_analyzer = std::make_unique<hms_colada::HabitAnalyzer>(db.get());

    // Model directory
    std::string model_dir = config.ml_training.model_dir;
    if (model_dir.empty()) {
        model_dir = config_dir + "/models";
    }
    std::filesystem::create_directories(model_dir);

    auto identifier = std::make_unique<hms_colada::HybridEngine>(db.get(), model_dir);

    // ── MQTT ────────────────────────────────────────────────────────────
    std::unique_ptr<hms::MqttClient> mqtt;
    std::unique_ptr<hms_colada::DiscoveryPublisher> discovery;
    std::unique_ptr<hms_colada::ScaleSubscriber> subscriber;

    if (config.mqtt.enabled) {
        hms::MqttConfig mqtt_cfg;
        mqtt_cfg.broker = config.mqtt.broker;
        mqtt_cfg.port = config.mqtt.port;
        mqtt_cfg.username = config.mqtt.username;
        mqtt_cfg.password = config.mqtt.password;
        mqtt_cfg.client_id = config.mqtt.client_id;

        mqtt = std::make_unique<hms::MqttClient>(mqtt_cfg);
        if (mqtt->connect()) {
            spdlog::info("Connected to MQTT broker at {}:{}", config.mqtt.broker, config.mqtt.port);

            discovery = std::make_unique<hms_colada::DiscoveryPublisher>(mqtt.get());
            discovery->publishAvailability(true);

            // Publish discovery for all active users
            auto users = db->getUsers(true);
            for (const auto& user : users) {
                discovery->publishUserDiscovery(user);
            }
            discovery->publishSystemDiscovery(users);
            spdlog::info("Published HA discovery for {} users", users.size());

            subscriber = std::make_unique<hms_colada::ScaleSubscriber>(
                mqtt.get(), db.get(), discovery.get(),
                identifier.get(), calculator.get());
            subscriber->start();
            spdlog::info("MQTT subscriber started (topic: {})", config.mqtt.scale_topic);
        } else {
            spdlog::warn("Failed to connect to MQTT broker, continuing without MQTT");
        }
    }

    // ── ML Training Service ─────────────────────────────────────────────
    std::unique_ptr<hms_colada::MLTrainingService> ml_service;

    if (config.ml_training.enabled) {
        // Separate DB connection for ML thread (pqxx not thread-safe)
        auto ml_db = std::make_unique<PostgresScaleDatabase>(config.connectionString());
        if (ml_db->connect()) {
            hms_colada::MLTrainingService::Config ml_cfg;
            ml_cfg.enabled = true;
            ml_cfg.schedule = config.ml_training.schedule;
            ml_cfg.model_dir = model_dir;
            ml_cfg.min_measurements = config.ml_training.min_measurements;

            ml_service = std::make_unique<hms_colada::MLTrainingService>(
                ml_db.release(), ml_cfg);
            ml_service->start();
            spdlog::info("ML Training: enabled (schedule: {})", config.ml_training.schedule);
        } else {
            spdlog::warn("Failed to create ML DB connection, ML training disabled");
        }
    }

    // ── Web Server (Drogon) ─────────────────────────────────────────────
#ifdef BUILD_WITH_WEB
    ColadaController::setDatabase(db.get());
    ColadaController::setConfigPath(config_path);
    ColadaController::setHabitAnalyzer(habit_analyzer.get());
    ColadaController::setIdentifier(identifier.get());
    ColadaController::setCalculator(calculator.get());

    if (ml_service) {
        ColadaController::setMlTrainTrigger([&ml_service]() {
            ml_service->triggerTraining();
        });
        ColadaController::setMlStatusGetter([&ml_service]() -> Json::Value {
            return ml_service->getStatus();
        });
    }

    // SPA fallback: serve index.html for Angular routes
    std::string index_path = config.static_dir + "/index.html";
    std::string index_html;
    if (std::filesystem::exists(index_path)) {
        std::ifstream ifs(index_path);
        index_html = std::string(std::istreambuf_iterator<char>(ifs),
                                  std::istreambuf_iterator<char>());
        spdlog::info("Loaded SPA index.html ({} bytes)", index_html.size());
    }

    if (!index_html.empty()) {
        drogon::app().setCustomErrorHandler(
            [index_html](drogon::HttpStatusCode code) -> drogon::HttpResponsePtr {
                if (code == drogon::k404NotFound) {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setContentTypeCode(drogon::CT_TEXT_HTML);
                    resp->setBody(index_html);
                    return resp;
                }
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(code);
                return resp;
            });
    }

    drogon::app()
        .setLogLevel(trantor::Logger::kWarn)
        .addListener("0.0.0.0", config.web_port)
        .setDocumentRoot(config.static_dir)
        .setStaticFilesCacheTime(3600);

    spdlog::info("Starting web server on port {}", config.web_port);
    spdlog::info("hms-colada v1.0.0 ready");
    drogon::app().run();
#else
    spdlog::info("hms-colada v1.0.0 ready (no web server)");
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
#endif

    // ── Shutdown ────────────────────────────────────────────────────────
    spdlog::info("Shutting down...");

    if (ml_service) ml_service->stop();
    if (mqtt) {
        if (discovery) discovery->publishAvailability(false);
    }

    spdlog::info("hms-colada stopped");
    return 0;
}
