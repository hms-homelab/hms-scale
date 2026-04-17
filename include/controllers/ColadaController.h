#pragma once
#ifdef BUILD_WITH_WEB

#include <drogon/HttpController.h>
#include <json/json.h>
#include <functional>
#include <memory>

// Forward declarations
class IScaleDatabase;
namespace hms_colada {
class HybridEngine;
class MLTrainingService;
class HabitAnalyzer;
class BodyCompositionCalculator;
}

class ColadaController : public drogon::HttpController<ColadaController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ColadaController::health,              "/health",                          drogon::Get);
    ADD_METHOD_TO(ColadaController::dashboard,           "/api/dashboard",                   drogon::Get);
    // Users
    ADD_METHOD_TO(ColadaController::getUsers,            "/api/users",                       drogon::Get);
    ADD_METHOD_TO(ColadaController::createUser,          "/api/users",                       drogon::Post);
    ADD_METHOD_TO(ColadaController::getUser,             "/api/users/{id}",                  drogon::Get);
    ADD_METHOD_TO(ColadaController::updateUser,          "/api/users/{id}",                  drogon::Put);
    ADD_METHOD_TO(ColadaController::deleteUser,          "/api/users/{id}",                  drogon::Delete);
    // Measurements
    ADD_METHOD_TO(ColadaController::getMeasurements,     "/api/measurements",                drogon::Get);
    ADD_METHOD_TO(ColadaController::getUnassigned,       "/api/measurements/unassigned",     drogon::Get);
    ADD_METHOD_TO(ColadaController::assignMeasurement,   "/api/measurements/{id}/assign",    drogon::Post);
    // ML
    ADD_METHOD_TO(ColadaController::triggerMlTrain,      "/api/ml/train",                    drogon::Post);
    ADD_METHOD_TO(ColadaController::mlStatus,            "/api/ml/status",                   drogon::Get);
    ADD_METHOD_TO(ColadaController::mlPredict,           "/api/ml/predict",                  drogon::Post);
    // Analytics
    ADD_METHOD_TO(ColadaController::dailyAverages,       "/api/analytics/daily",             drogon::Get);
    ADD_METHOD_TO(ColadaController::weeklyTrends,        "/api/analytics/weekly",            drogon::Get);
    ADD_METHOD_TO(ColadaController::summary,             "/api/analytics/summary",           drogon::Get);
    ADD_METHOD_TO(ColadaController::progress,            "/api/analytics/progress",          drogon::Get);
    // Habits
    ADD_METHOD_TO(ColadaController::habitInsights,       "/api/habits/insights",             drogon::Get);
    ADD_METHOD_TO(ColadaController::habitPredictions,    "/api/habits/predictions",          drogon::Get);
    // Config
    ADD_METHOD_TO(ColadaController::getConfig,           "/api/config",                      drogon::Get);
    ADD_METHOD_TO(ColadaController::updateConfig,        "/api/config",                      drogon::Put);
    METHOD_LIST_END

    // Handler signatures
    void health(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void dashboard(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void getUsers(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void createUser(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void getUser(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb, const std::string& id);
    void updateUser(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb, const std::string& id);
    void deleteUser(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb, const std::string& id);
    void getMeasurements(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void getUnassigned(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void assignMeasurement(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb, const std::string& id);
    void triggerMlTrain(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void mlStatus(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void mlPredict(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void dailyAverages(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void weeklyTrends(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void summary(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void progress(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void habitInsights(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void habitPredictions(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void getConfig(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void updateConfig(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    // Static setters for dependency injection (called from main.cpp)
    static void setDatabase(IScaleDatabase* db) { db_ = db; }
    static void setConfigPath(const std::string& path) { config_path_ = path; }
    static void setMlTrainTrigger(std::function<void()> fn) { ml_train_trigger_ = fn; }
    static void setMlStatusGetter(std::function<Json::Value()> fn) { ml_status_getter_ = fn; }
    static void setHabitAnalyzer(hms_colada::HabitAnalyzer* ha) { habit_analyzer_ = ha; }
    static void setIdentifier(hms_colada::HybridEngine* id) { identifier_ = id; }
    static void setCalculator(hms_colada::BodyCompositionCalculator* calc) { calculator_ = calc; }

private:
    static Json::Value jsonResp(const Json::Value& data);
    static drogon::HttpResponsePtr jsonError(const std::string& msg, drogon::HttpStatusCode code);

    static IScaleDatabase* db_;
    static std::function<void()> ml_train_trigger_;
    static std::function<Json::Value()> ml_status_getter_;
    static hms_colada::HabitAnalyzer* habit_analyzer_;
    static hms_colada::HybridEngine* identifier_;
    static hms_colada::BodyCompositionCalculator* calculator_;
    static std::string config_path_;
};

#endif // BUILD_WITH_WEB
