#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

struct AppConfig {
    // Web
    int web_port = 8889;
    std::string static_dir = "./static/browser";

    // Database (PostgreSQL only)
    struct Database {
        std::string host = "localhost";
        int port = 5432;
        std::string name = "colada_scale";
        std::string user = "colada_user";
        std::string password;
    } database;

    // MQTT
    struct Mqtt {
        bool enabled = true;
        std::string broker = "localhost";
        int port = 1883;
        std::string username;
        std::string password;
        std::string client_id = "hms_colada";
        std::string scale_topic = "giraffe_scale/measurement";
        std::string user_selector_topic = "colada_scale/user_selector/set";
    } mqtt;

    // ML Training
    struct MlTraining {
        bool enabled = false;
        std::string schedule = "weekly"; // daily, weekly, monthly
        std::string model_dir;           // default: ~/.hms-colada/models/
        int min_measurements = 20;       // per user minimum
    } ml_training;

    bool setup_complete = false;

    /// Fill empty config fields from environment variables (fallback).
    /// Call AFTER loading config.json so file values take precedence.
    void applyEnvFallbacks() {
        auto env = [](const char* name) -> std::string {
            const char* v = std::getenv(name);
            return v ? v : "";
        };
        auto envInt = [&](const char* name, int def) -> int {
            auto s = env(name);
            return s.empty() ? def : std::stoi(s);
        };

        // Web
        {
            int v = envInt("WEB_PORT", 0);
            if (v == 0) v = envInt("COLADA_PORT", 0);
            if (v > 0) web_port = v;
        }
        {
            auto v = env("STATIC_DIR");
            if (!v.empty()) static_dir = v;
        }

        // Database
        if (database.host == "localhost") {
            auto v = env("DB_HOST");
            if (!v.empty()) database.host = v;
        }
        if (database.port == 5432) {
            int v = envInt("DB_PORT", 0);
            if (v > 0) database.port = v;
        }
        if (database.name == "colada_scale") {
            auto v = env("DB_NAME");
            if (!v.empty()) database.name = v;
        }
        if (database.user == "colada_user") {
            auto v = env("DB_USER");
            if (!v.empty()) database.user = v;
        }
        if (database.password.empty()) database.password = env("DB_PASSWORD");

        // MQTT
        if (mqtt.broker == "localhost") {
            auto v = env("MQTT_BROKER");
            if (!v.empty()) mqtt.broker = v;
        }
        if (mqtt.port == 1883) {
            int v = envInt("MQTT_PORT", 0);
            if (v > 0) mqtt.port = v;
        }
        if (mqtt.username.empty())  mqtt.username  = env("MQTT_USER");
        if (mqtt.password.empty())  mqtt.password  = env("MQTT_PASSWORD");
        if (mqtt.client_id == "hms_colada") {
            auto v = env("MQTT_CLIENT_ID");
            if (!v.empty()) mqtt.client_id = v;
        }
        if (mqtt.scale_topic == "giraffe_scale/measurement") {
            auto v = env("SCALE_TOPIC");
            if (!v.empty()) mqtt.scale_topic = v;
        }
        if (mqtt.user_selector_topic == "colada_scale/user_selector/set") {
            auto v = env("USER_SELECTOR_TOPIC");
            if (!v.empty()) mqtt.user_selector_topic = v;
        }

        // ML Training
        if (!ml_training.enabled && env("ML_ENABLED") == "true")
            ml_training.enabled = true;
        if (ml_training.schedule == "weekly") {
            auto v = env("ML_SCHEDULE");
            if (!v.empty()) ml_training.schedule = v;
        }
        if (ml_training.model_dir.empty()) ml_training.model_dir = env("ML_MODEL_DIR");
        if (ml_training.min_measurements == 20) {
            int v = envInt("ML_MIN_MEASUREMENTS", 0);
            if (v > 0) ml_training.min_measurements = v;
        }
    }

    // Get default data directory (~/.hms-colada/)
    static std::string dataDir() {
        std::string home;
        #ifdef _WIN32
        home = std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : "C:\\";
        return home + "\\.hms-colada";
        #else
        home = std::getenv("HOME") ? std::getenv("HOME") : "/tmp";
        return home + "/.hms-colada";
        #endif
    }

    // PostgreSQL connection string
    std::string connectionString() const {
        return "host=" + database.host +
               " port=" + std::to_string(database.port) +
               " dbname=" + database.name +
               " user=" + database.user +
               " password=" + database.password;
    }

    // Load from JSON file
    static bool load(const std::string& path, AppConfig& config) {
        if (!std::filesystem::exists(path)) return false;
        try {
            std::ifstream f(path);
            nlohmann::json j;
            f >> j;

            if (j.contains("web_port"))       config.web_port = j["web_port"];
            if (j.contains("static_dir"))     config.static_dir = j["static_dir"];
            if (j.contains("setup_complete")) config.setup_complete = j["setup_complete"];

            if (j.contains("database")) {
                auto& d = j["database"];
                if (d.contains("host"))               config.database.host = d["host"];
                if (d.contains("port"))               config.database.port = d["port"];
                if (d.contains("name"))               config.database.name = d["name"];
                if (d.contains("user"))               config.database.user = d["user"];
                if (d.contains("password"))           config.database.password = d["password"];
            }

            if (j.contains("mqtt")) {
                auto& m = j["mqtt"];
                if (m.contains("enabled"))              config.mqtt.enabled = m["enabled"];
                if (m.contains("broker"))               config.mqtt.broker = m["broker"];
                if (m.contains("port"))                 config.mqtt.port = m["port"];
                if (m.contains("username"))             config.mqtt.username = m["username"];
                if (m.contains("password"))             config.mqtt.password = m["password"];
                if (m.contains("client_id"))            config.mqtt.client_id = m["client_id"];
                if (m.contains("scale_topic"))          config.mqtt.scale_topic = m["scale_topic"];
                if (m.contains("user_selector_topic"))  config.mqtt.user_selector_topic = m["user_selector_topic"];
            }

            if (j.contains("ml_training")) {
                auto& ml = j["ml_training"];
                if (ml.contains("enabled"))           config.ml_training.enabled = ml["enabled"];
                if (ml.contains("schedule"))          config.ml_training.schedule = ml["schedule"];
                if (ml.contains("model_dir"))         config.ml_training.model_dir = ml["model_dir"];
                if (ml.contains("min_measurements"))  config.ml_training.min_measurements = ml["min_measurements"];
            }

            return true;
        } catch (const std::exception& e) {
            std::cerr << "Config load error: " << e.what() << std::endl;
            return false;
        }
    }

    // Save to JSON file
    bool save(const std::string& path) const {
        try {
            std::filesystem::create_directories(std::filesystem::path(path).parent_path());
            nlohmann::json j;
            j["web_port"] = web_port;
            j["static_dir"] = static_dir;
            j["setup_complete"] = setup_complete;

            j["database"]["host"] = database.host;
            j["database"]["port"] = database.port;
            j["database"]["name"] = database.name;
            j["database"]["user"] = database.user;
            j["database"]["password"] = database.password;

            j["mqtt"]["enabled"] = mqtt.enabled;
            j["mqtt"]["broker"] = mqtt.broker;
            j["mqtt"]["port"] = mqtt.port;
            j["mqtt"]["username"] = mqtt.username;
            j["mqtt"]["password"] = mqtt.password;
            j["mqtt"]["client_id"] = mqtt.client_id;
            j["mqtt"]["scale_topic"] = mqtt.scale_topic;
            j["mqtt"]["user_selector_topic"] = mqtt.user_selector_topic;

            j["ml_training"]["enabled"] = ml_training.enabled;
            j["ml_training"]["schedule"] = ml_training.schedule;
            j["ml_training"]["model_dir"] = ml_training.model_dir;
            j["ml_training"]["min_measurements"] = ml_training.min_measurements;

            std::ofstream f(path);
            f << j.dump(2);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Config save error: " << e.what() << std::endl;
            return false;
        }
    }

    // Convert to JSON for API (passwords redacted)
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["web_port"] = web_port;
        j["static_dir"] = static_dir;
        j["setup_complete"] = setup_complete;

        j["database"]["host"] = database.host;
        j["database"]["port"] = database.port;
        j["database"]["name"] = database.name;
        j["database"]["user"] = database.user;
        j["database"]["password"] = database.password.empty() ? "" : "********";

        j["mqtt"]["enabled"] = mqtt.enabled;
        j["mqtt"]["broker"] = mqtt.broker;
        j["mqtt"]["port"] = mqtt.port;
        j["mqtt"]["username"] = mqtt.username;
        j["mqtt"]["password"] = mqtt.password.empty() ? "" : "********";
        j["mqtt"]["client_id"] = mqtt.client_id;
        j["mqtt"]["scale_topic"] = mqtt.scale_topic;
        j["mqtt"]["user_selector_topic"] = mqtt.user_selector_topic;

        j["ml_training"]["enabled"] = ml_training.enabled;
        j["ml_training"]["schedule"] = ml_training.schedule;
        j["ml_training"]["model_dir"] = ml_training.model_dir;
        j["ml_training"]["min_measurements"] = ml_training.min_measurements;

        return j;
    }
};
