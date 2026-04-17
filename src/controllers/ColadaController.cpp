#ifdef BUILD_WITH_WEB

#include "controllers/ColadaController.h"
#include "database/IScaleDatabase.h"
#include "models/ScaleModels.h"
#include "identification/HybridEngine.h"
#include "analytics/HabitAnalyzer.h"
#include "calculation/BodyCompositionCalculator.h"
#include "mqtt/ScaleSubscriber.h"
#include "utils/AppConfig.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

// Static member initialization
IScaleDatabase* ColadaController::db_ = nullptr;
std::function<void()> ColadaController::ml_train_trigger_;
std::function<Json::Value()> ColadaController::ml_status_getter_;
hms_colada::HabitAnalyzer* ColadaController::habit_analyzer_ = nullptr;
hms_colada::HybridEngine* ColadaController::identifier_ = nullptr;
hms_colada::BodyCompositionCalculator* ColadaController::calculator_ = nullptr;
hms_colada::ScaleSubscriber* ColadaController::subscriber_ = nullptr;
std::string ColadaController::config_path_;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Convert nlohmann::json to jsoncpp Json::Value via serialization
static Json::Value nlohmannToJsoncpp(const nlohmann::json& nj) {
    Json::Value root;
    Json::CharReaderBuilder rb;
    std::string errs;
    std::string s = nj.dump();
    std::istringstream ss(s);
    Json::parseFromStream(rb, ss, &root, &errs);
    return root;
}

// Bypass Drogon's newHttpJsonResponse (crashes in cross-compiled ARM binary)
static drogon::HttpResponsePtr makeJsonResponse(const Json::Value& val) {
    Json::StreamWriterBuilder wb;
    wb["indentation"] = "";
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->setBody(Json::writeString(wb, val));
    return resp;
}

drogon::HttpResponsePtr ColadaController::jsonError(const std::string& msg,
                                                     drogon::HttpStatusCode code) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->setStatusCode(code);
    resp->setBody("{\"error\":\"" + msg + "\"}");
    return resp;
}

Json::Value ColadaController::jsonResp(const Json::Value& data) {
    // Wrap data — caller still needs to pass through makeJsonResponse
    return data;
}

// Check that db_ is available, return 503 if not
#define CHECK_DB(cb) \
    if (!db_) { cb(jsonError("service unavailable", drogon::k503ServiceUnavailable)); return; }

// Safe int parameter parsing
static int intParam(const drogon::HttpRequestPtr& req, const std::string& name, int def) {
    auto val = req->getParameter(name);
    if (val.empty()) return def;
    try { return std::stoi(val); }
    catch (...) { return def; }
}

// ---------------------------------------------------------------------------
// Health
// ---------------------------------------------------------------------------

void ColadaController::health(const drogon::HttpRequestPtr&,
                               std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    Json::Value j;
    j["status"] = "ok";
    j["service"] = "hms-colada";
    j["version"] = "1.0.0";
    cb(makeJsonResponse(j));
}

// ---------------------------------------------------------------------------
// Dashboard
// ---------------------------------------------------------------------------

void ColadaController::dashboard(const drogon::HttpRequestPtr&,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    CHECK_DB(cb);
    try {
        auto users = db_->getUsers(true);
        Json::Value usersArr(Json::arrayValue);
        int total_measurements = 0;

        for (const auto& u : users) {
            nlohmann::json nj = u;
            Json::Value uj = nlohmannToJsoncpp(nj);

            auto latest = db_->getLatestByUser(u.id);
            if (latest) {
                nlohmann::json mj = *latest;
                uj["latest_measurement"] = nlohmannToJsoncpp(mj);
            } else {
                uj["latest_measurement"] = Json::nullValue;
            }

            int count = db_->getMeasurementCount(u.id);
            uj["measurement_count"] = count;
            total_measurements += count;

            usersArr.append(uj);
        }

        Json::Value resp;
        resp["users"] = usersArr;
        resp["total_users"] = static_cast<int>(users.size());
        resp["total_measurements"] = total_measurements;
        resp["service"] = "hms-colada";
        cb(makeJsonResponse(resp));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

// ---------------------------------------------------------------------------
// Users
// ---------------------------------------------------------------------------

void ColadaController::getUsers(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    CHECK_DB(cb);
    try {
        auto active_str = req->getParameter("active_only");
        bool active_only = (active_str.empty() || active_str == "true");
        auto users = db_->getUsers(active_only);

        Json::Value arr(Json::arrayValue);
        for (const auto& u : users) {
            nlohmann::json nj = u;
            arr.append(nlohmannToJsoncpp(nj));
        }

        Json::Value resp;
        resp["users"] = arr;
        resp["count"] = static_cast<int>(users.size());
        cb(makeJsonResponse(resp));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

void ColadaController::createUser(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    CHECK_DB(cb);
    try {
        auto body = req->getBody();
        if (body.empty()) {
            cb(jsonError("empty request body", drogon::k400BadRequest));
            return;
        }

        nlohmann::json nj = nlohmann::json::parse(std::string(body));

        ScaleUser user;
        user.name = nj.value("name", "");
        user.date_of_birth = nj.value("date_of_birth", "");
        user.sex = nj.value("sex", "");
        user.height_cm = nj.value("height_cm", 0.0);
        user.expected_weight_kg = nj.value("expected_weight_kg", 0.0);
        user.weight_tolerance_kg = nj.value("weight_tolerance_kg", 3.0);

        if (user.name.empty()) {
            cb(jsonError("name is required", drogon::k400BadRequest));
            return;
        }

        auto created = db_->createUser(user);
        if (!created) {
            cb(jsonError("failed to create user", drogon::k500InternalServerError));
            return;
        }

        nlohmann::json cj = *created;
        cb(makeJsonResponse(nlohmannToJsoncpp(cj)));
    } catch (const nlohmann::json::exception& e) {
        cb(jsonError(std::string("invalid JSON: ") + e.what(), drogon::k400BadRequest));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

void ColadaController::getUser(const drogon::HttpRequestPtr&,
                                std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                const std::string& id) {
    CHECK_DB(cb);
    try {
        auto user = db_->getUser(id);
        if (!user) {
            cb(jsonError("user not found", drogon::k404NotFound));
            return;
        }

        nlohmann::json nj = *user;
        cb(makeJsonResponse(nlohmannToJsoncpp(nj)));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

void ColadaController::updateUser(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                   const std::string& id) {
    CHECK_DB(cb);
    try {
        auto body = req->getBody();
        if (body.empty()) {
            cb(jsonError("empty request body", drogon::k400BadRequest));
            return;
        }

        nlohmann::json fields = nlohmann::json::parse(std::string(body));

        if (!db_->updateUser(id, fields)) {
            cb(jsonError("user not found or update failed", drogon::k404NotFound));
            return;
        }

        // Return updated user
        auto updated = db_->getUser(id);
        if (updated) {
            nlohmann::json nj = *updated;
            cb(makeJsonResponse(nlohmannToJsoncpp(nj)));
        } else {
            Json::Value resp;
            resp["status"] = "updated";
            cb(makeJsonResponse(resp));
        }
    } catch (const nlohmann::json::exception& e) {
        cb(jsonError(std::string("invalid JSON: ") + e.what(), drogon::k400BadRequest));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

void ColadaController::deleteUser(const drogon::HttpRequestPtr&,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                   const std::string& id) {
    CHECK_DB(cb);
    try {
        if (!db_->deleteUser(id)) {
            cb(jsonError("user not found", drogon::k404NotFound));
            return;
        }

        Json::Value resp;
        resp["status"] = "deleted";
        resp["id"] = id;
        cb(makeJsonResponse(resp));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

// ---------------------------------------------------------------------------
// Measurements
// ---------------------------------------------------------------------------

void ColadaController::getMeasurements(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    CHECK_DB(cb);
    try {
        auto user_id = req->getParameter("user_id");
        int days = intParam(req, "days", 30);
        int limit = intParam(req, "limit", 100);
        int offset = intParam(req, "offset", 0);

        if (user_id.empty()) {
            cb(jsonError("user_id parameter required", drogon::k400BadRequest));
            return;
        }

        auto measurements = db_->getMeasurements(user_id, days, limit, offset);

        Json::Value arr(Json::arrayValue);
        for (const auto& m : measurements) {
            nlohmann::json nj = m;
            arr.append(nlohmannToJsoncpp(nj));
        }

        Json::Value resp;
        resp["measurements"] = arr;
        resp["count"] = static_cast<int>(measurements.size());
        cb(makeJsonResponse(resp));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

void ColadaController::getUnassigned(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    CHECK_DB(cb);
    try {
        int limit = intParam(req, "limit", 50);
        auto measurements = db_->getUnassigned(limit);

        Json::Value arr(Json::arrayValue);
        for (const auto& m : measurements) {
            nlohmann::json nj = m;
            arr.append(nlohmannToJsoncpp(nj));
        }

        Json::Value resp;
        resp["measurements"] = arr;
        resp["count"] = static_cast<int>(measurements.size());
        cb(makeJsonResponse(resp));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

void ColadaController::assignMeasurement(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                                          const std::string& id) {
    CHECK_DB(cb);
    try {
        auto body = req->getBody();
        if (body.empty()) {
            cb(jsonError("empty request body", drogon::k400BadRequest));
            return;
        }

        nlohmann::json nj = nlohmann::json::parse(std::string(body));
        auto user_id = nj.value("user_id", "");
        double confidence = nj.value("confidence", 100.0);

        if (user_id.empty()) {
            cb(jsonError("user_id is required", drogon::k400BadRequest));
            return;
        }

        if (!db_->assignMeasurement(id, user_id, confidence)) {
            cb(jsonError("measurement not found or assign failed", drogon::k404NotFound));
            return;
        }

        Json::Value resp;
        resp["status"] = "assigned";
        resp["measurement_id"] = id;
        resp["user_id"] = user_id;
        resp["confidence"] = confidence;
        cb(makeJsonResponse(resp));
    } catch (const nlohmann::json::exception& e) {
        cb(jsonError(std::string("invalid JSON: ") + e.what(), drogon::k400BadRequest));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

// ---------------------------------------------------------------------------
// ML
// ---------------------------------------------------------------------------

void ColadaController::triggerMlTrain(const drogon::HttpRequestPtr&,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    if (!ml_train_trigger_) {
        cb(jsonError("ML training not configured", drogon::k503ServiceUnavailable));
        return;
    }
    try {
        ml_train_trigger_();
        Json::Value resp;
        resp["status"] = "training_started";
        cb(makeJsonResponse(resp));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

void ColadaController::mlStatus(const drogon::HttpRequestPtr&,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    if (!ml_status_getter_) {
        cb(jsonError("ML status not configured", drogon::k503ServiceUnavailable));
        return;
    }
    try {
        cb(makeJsonResponse(ml_status_getter_()));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

void ColadaController::mlPredict(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    if (!identifier_) {
        cb(jsonError("identifier not configured", drogon::k503ServiceUnavailable));
        return;
    }
    try {
        auto body = req->getBody();
        if (body.empty()) {
            cb(jsonError("empty request body", drogon::k400BadRequest));
            return;
        }

        nlohmann::json nj = nlohmann::json::parse(std::string(body));
        double weight_kg = nj.value("weight_kg", 0.0);
        double impedance_ohm = nj.value("impedance_ohm", 0.0);

        if (weight_kg <= 0) {
            cb(jsonError("weight_kg must be positive", drogon::k400BadRequest));
            return;
        }

        auto result = identifier_->identify(weight_kg, impedance_ohm);

        Json::Value resp;
        if (result) {
            resp["identified"] = true;
            resp["user_id"] = result->user_id;
            resp["user_name"] = result->user_name;
            resp["confidence"] = result->confidence;
            resp["method"] = result->method;
            resp["requires_manual"] = result->requires_manual;

            Json::Value probs;
            for (const auto& [name, prob] : result->ml_probabilities) {
                probs[name] = prob;
            }
            resp["ml_probabilities"] = probs;
        } else {
            resp["identified"] = false;
            resp["requires_manual"] = true;
        }
        cb(makeJsonResponse(resp));
    } catch (const nlohmann::json::exception& e) {
        cb(jsonError(std::string("invalid JSON: ") + e.what(), drogon::k400BadRequest));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

// ---------------------------------------------------------------------------
// Analytics
// ---------------------------------------------------------------------------

void ColadaController::dailyAverages(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    CHECK_DB(cb);
    try {
        auto user_id = req->getParameter("user_id");
        int days = intParam(req, "days", 30);

        if (user_id.empty()) {
            cb(jsonError("user_id parameter required", drogon::k400BadRequest));
            return;
        }

        auto averages = db_->getDailyAverages(user_id, days);

        Json::Value arr(Json::arrayValue);
        for (const auto& d : averages) {
            nlohmann::json nj = d;
            arr.append(nlohmannToJsoncpp(nj));
        }

        Json::Value resp;
        resp["daily_averages"] = arr;
        resp["count"] = static_cast<int>(averages.size());
        resp["user_id"] = user_id;
        resp["days"] = days;
        cb(makeJsonResponse(resp));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

void ColadaController::weeklyTrends(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    CHECK_DB(cb);
    try {
        auto user_id = req->getParameter("user_id");
        int weeks = intParam(req, "weeks", 12);

        if (user_id.empty()) {
            cb(jsonError("user_id parameter required", drogon::k400BadRequest));
            return;
        }

        auto trends = db_->getWeeklyTrends(user_id, weeks);

        Json::Value arr(Json::arrayValue);
        for (const auto& t : trends) {
            nlohmann::json nj = t;
            arr.append(nlohmannToJsoncpp(nj));
        }

        Json::Value resp;
        resp["weekly_trends"] = arr;
        resp["count"] = static_cast<int>(trends.size());
        resp["user_id"] = user_id;
        resp["weeks"] = weeks;
        cb(makeJsonResponse(resp));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

void ColadaController::summary(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    CHECK_DB(cb);
    try {
        auto user_id = req->getParameter("user_id");
        if (user_id.empty()) {
            cb(jsonError("user_id parameter required", drogon::k400BadRequest));
            return;
        }

        auto user = db_->getUser(user_id);
        if (!user) {
            cb(jsonError("user not found", drogon::k404NotFound));
            return;
        }

        Json::Value resp;

        // User info
        nlohmann::json uj = *user;
        resp["user"] = nlohmannToJsoncpp(uj);

        // Latest measurement
        auto latest = db_->getLatestByUser(user_id);
        if (latest) {
            nlohmann::json mj = *latest;
            resp["latest_measurement"] = nlohmannToJsoncpp(mj);
        } else {
            resp["latest_measurement"] = Json::nullValue;
        }

        // 7-day stats
        auto daily_7d = db_->getDailyAverages(user_id, 7);
        if (!daily_7d.empty()) {
            double sum = 0;
            for (const auto& d : daily_7d) sum += d.avg_weight_kg;
            resp["avg_weight_7d"] = sum / daily_7d.size();
            resp["measurements_7d"] = 0;
            for (const auto& d : daily_7d) resp["measurements_7d"] = resp["measurements_7d"].asInt() + d.measurement_count;
        }

        // 30-day stats
        auto daily_30d = db_->getDailyAverages(user_id, 30);
        if (!daily_30d.empty()) {
            double sum = 0;
            for (const auto& d : daily_30d) sum += d.avg_weight_kg;
            resp["avg_weight_30d"] = sum / daily_30d.size();
            resp["measurements_30d"] = 0;
            for (const auto& d : daily_30d) resp["measurements_30d"] = resp["measurements_30d"].asInt() + d.measurement_count;

            // Weight change (first day avg vs last day avg)
            resp["weight_change_30d"] = daily_30d.back().avg_weight_kg - daily_30d.front().avg_weight_kg;
        }

        resp["total_measurements"] = db_->getMeasurementCount(user_id);
        cb(makeJsonResponse(resp));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

void ColadaController::progress(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    CHECK_DB(cb);
    try {
        auto user_id = req->getParameter("user_id");
        int days = intParam(req, "days", 90);
        auto metric = req->getParameter("metric");
        if (metric.empty()) metric = "weight";

        if (user_id.empty()) {
            cb(jsonError("user_id parameter required", drogon::k400BadRequest));
            return;
        }

        auto averages = db_->getDailyAverages(user_id, days);

        // Build Chart.js-compatible time series
        Json::Value labels(Json::arrayValue);
        Json::Value data(Json::arrayValue);

        for (const auto& d : averages) {
            labels.append(d.date);
            if (metric == "weight") {
                data.append(d.avg_weight_kg);
            } else if (metric == "body_fat") {
                data.append(d.avg_body_fat);
            } else if (metric == "measurement_count") {
                data.append(d.measurement_count);
            } else {
                data.append(d.avg_weight_kg);
            }
        }

        Json::Value resp;
        resp["labels"] = labels;
        resp["data"] = data;
        resp["metric"] = metric;
        resp["user_id"] = user_id;
        resp["days"] = days;
        cb(makeJsonResponse(resp));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

// ---------------------------------------------------------------------------
// Habits
// ---------------------------------------------------------------------------

void ColadaController::habitInsights(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    if (!habit_analyzer_) {
        cb(jsonError("habit analyzer not configured", drogon::k503ServiceUnavailable));
        return;
    }
    try {
        auto user_id = req->getParameter("user_id");
        int days = intParam(req, "days", 90);

        if (user_id.empty()) {
            cb(jsonError("user_id parameter required", drogon::k400BadRequest));
            return;
        }

        auto insights = habit_analyzer_->getInsights(user_id, days);

        // Manually build jsoncpp response from HabitInsights
        Json::Value resp;

        // Consistency
        Json::Value cons;
        cons["total_measurements"] = insights.consistency.total_measurements;
        cons["avg_days_between"] = insights.consistency.avg_days_between;
        cons["longest_gap_days"] = insights.consistency.longest_gap_days;
        cons["consistency_score"] = insights.consistency.consistency_score;
        cons["max_streak_days"] = insights.consistency.max_streak_days;
        cons["current_streak_days"] = insights.consistency.current_streak_days;
        resp["consistency"] = cons;

        // Weight trend
        Json::Value wt;
        wt["current_weight"] = insights.weight.current_weight;
        wt["starting_weight"] = insights.weight.starting_weight;
        wt["total_change_kg"] = insights.weight.total_change_kg;
        wt["min_weight"] = insights.weight.min_weight;
        wt["max_weight"] = insights.weight.max_weight;
        wt["weight_range"] = insights.weight.weight_range;
        wt["trend_direction"] = insights.weight.trend_direction;
        wt["slope_kg_per_measurement"] = insights.weight.slope_kg_per_measurement;
        wt["volatility"] = insights.weight.volatility;
        wt["is_volatile"] = insights.weight.is_volatile;
        wt["avg_7d"] = insights.weight.avg_7d;
        wt["avg_30d"] = insights.weight.avg_30d;
        resp["weight"] = wt;

        // Body comp trend
        Json::Value bc;
        bc["body_fat_change"] = insights.body_comp.body_fat_change;
        bc["muscle_change_kg"] = insights.body_comp.muscle_change_kg;
        bc["body_fat_trend"] = insights.body_comp.body_fat_trend;
        bc["muscle_trend"] = insights.body_comp.muscle_trend;
        bc["is_recomposing"] = insights.body_comp.is_recomposing;
        resp["body_comp"] = bc;

        // Weekly pattern
        Json::Value wp;
        Json::Value dayAvgs;
        for (const auto& [day, avg] : insights.weekly.day_averages) {
            dayAvgs[day] = avg;
        }
        wp["day_averages"] = dayAvgs;
        wp["heaviest_day"] = insights.weekly.heaviest_day;
        wp["lightest_day"] = insights.weekly.lightest_day;
        wp["weekly_variation_kg"] = insights.weekly.weekly_variation_kg;
        wp["has_weekend_effect"] = insights.weekly.has_weekend_effect;
        resp["weekly"] = wp;

        // Predictions
        Json::Value pred;
        pred["predicted_weight_7d"] = insights.predictions.predicted_weight_7d;
        pred["predicted_weight_30d"] = insights.predictions.predicted_weight_30d;
        pred["predicted_weight_90d"] = insights.predictions.predicted_weight_90d;
        pred["rate_kg_per_week"] = insights.predictions.rate_kg_per_week;
        pred["rate_lbs_per_week"] = insights.predictions.rate_lbs_per_week;
        pred["trend_confidence"] = insights.predictions.trend_confidence;
        resp["predictions"] = pred;

        // Recommendations
        Json::Value recs(Json::arrayValue);
        for (const auto& r : insights.recommendations) {
            Json::Value rj;
            rj["category"] = r.category;
            rj["priority"] = r.priority;
            rj["title"] = r.title;
            rj["message"] = r.message;
            rj["actionable_step"] = r.actionable_step;
            recs.append(rj);
        }
        resp["recommendations"] = recs;

        // Alerts
        Json::Value alerts(Json::arrayValue);
        for (const auto& a : insights.alerts) {
            Json::Value aj;
            aj["type"] = a.type;
            aj["severity"] = a.severity;
            aj["message"] = a.message;
            alerts.append(aj);
        }
        resp["alerts"] = alerts;

        resp["user_id"] = user_id;
        resp["days"] = days;
        cb(makeJsonResponse(resp));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

void ColadaController::habitPredictions(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    if (!habit_analyzer_) {
        cb(jsonError("habit analyzer not configured", drogon::k503ServiceUnavailable));
        return;
    }
    try {
        auto user_id = req->getParameter("user_id");
        int days = intParam(req, "days", 90);

        if (user_id.empty()) {
            cb(jsonError("user_id parameter required", drogon::k400BadRequest));
            return;
        }

        auto insights = habit_analyzer_->getInsights(user_id, days);

        Json::Value resp;
        resp["predicted_weight_7d"] = insights.predictions.predicted_weight_7d;
        resp["predicted_weight_30d"] = insights.predictions.predicted_weight_30d;
        resp["predicted_weight_90d"] = insights.predictions.predicted_weight_90d;
        resp["rate_kg_per_week"] = insights.predictions.rate_kg_per_week;
        resp["rate_lbs_per_week"] = insights.predictions.rate_lbs_per_week;
        resp["trend_confidence"] = insights.predictions.trend_confidence;
        resp["trend_direction"] = insights.weight.trend_direction;
        resp["user_id"] = user_id;
        resp["days"] = days;
        cb(makeJsonResponse(resp));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

void ColadaController::getConfig(const drogon::HttpRequestPtr&,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    if (config_path_.empty()) {
        cb(jsonError("Config path not set", drogon::k500InternalServerError));
        return;
    }

    AppConfig config;
    AppConfig::load(config_path_, config);

    auto nj = config.toJson();
    auto resp = nlohmannToJsoncpp(nj);

    resp["config_path"] = config_path_;
    resp["services"]["db_connected"] = (db_ != nullptr);
    resp["services"]["ml_enabled"] = static_cast<bool>(ml_train_trigger_);
    resp["services"]["habits_enabled"] = (habit_analyzer_ != nullptr);
    resp["services"]["identifier_enabled"] = (identifier_ != nullptr);

    cb(makeJsonResponse(resp));
}

void ColadaController::updateConfig(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    if (config_path_.empty()) {
        cb(jsonError("Config path not set", drogon::k500InternalServerError));
        return;
    }

    auto body = req->getBody();
    if (body.empty()) {
        cb(jsonError("Empty request body", drogon::k400BadRequest));
        return;
    }

    try {
        auto incoming = nlohmann::json::parse(body);

        AppConfig config;
        AppConfig::load(config_path_, config);

        // Database
        if (incoming.contains("database")) {
            auto& d = incoming["database"];
            if (d.contains("host")) config.database.host = d["host"];
            if (d.contains("port")) config.database.port = d["port"];
            if (d.contains("name")) config.database.name = d["name"];
            if (d.contains("user")) config.database.user = d["user"];
            if (d.contains("password") && d["password"].is_string()) {
                std::string pw = d["password"];
                if (!pw.empty() && pw != "********") {
                    config.database.password = pw;
                }
            }
        }

        // MQTT
        if (incoming.contains("mqtt")) {
            auto& m = incoming["mqtt"];
            if (m.contains("enabled")) config.mqtt.enabled = m["enabled"];
            if (m.contains("broker")) config.mqtt.broker = m["broker"];
            if (m.contains("port")) config.mqtt.port = m["port"];
            if (m.contains("username")) config.mqtt.username = m["username"];
            if (m.contains("password") && m["password"].is_string()) {
                std::string pw = m["password"];
                if (!pw.empty() && pw != "********") {
                    config.mqtt.password = pw;
                }
            }
            if (m.contains("client_id")) config.mqtt.client_id = m["client_id"];
            if (m.contains("scale_topic")) config.mqtt.scale_topic = m["scale_topic"];
        }

        // ML Training
        if (incoming.contains("ml_training")) {
            auto& ml = incoming["ml_training"];
            if (ml.contains("enabled")) config.ml_training.enabled = ml["enabled"];
            if (ml.contains("schedule")) config.ml_training.schedule = ml["schedule"];
            if (ml.contains("model_dir")) config.ml_training.model_dir = ml["model_dir"];
            if (ml.contains("min_measurements")) config.ml_training.min_measurements = ml["min_measurements"];
        }

        // Web
        if (incoming.contains("web_port")) config.web_port = incoming["web_port"];
        if (incoming.contains("static_dir")) config.static_dir = incoming["static_dir"];
        if (incoming.contains("setup_complete")) config.setup_complete = incoming["setup_complete"];

        config.save(config_path_);

        Json::Value resp;
        resp["status"] = "ok";
        resp["message"] = "Config saved. Restart service for changes to take effect.";
        resp["config_path"] = config_path_;
        resp["restart_required"] = true;
        cb(makeJsonResponse(resp));

    } catch (const nlohmann::json::exception& e) {
        cb(jsonError(std::string("Invalid JSON: ") + e.what(), drogon::k400BadRequest));
    } catch (const std::exception& e) {
        cb(jsonError(std::string("Failed to save config: ") + e.what(), drogon::k500InternalServerError));
    }
}

// ---------------------------------------------------------------------------
// Webhook — ESP32 direct HTTP measurement submission
// ---------------------------------------------------------------------------

void ColadaController::webhookMeasurement(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    if (!subscriber_) {
        cb(jsonError("Measurement processor not available", drogon::k503ServiceUnavailable));
        return;
    }

    auto body = req->getBody();
    if (body.empty()) {
        cb(jsonError("empty request body", drogon::k400BadRequest));
        return;
    }

    try {
        auto nj = nlohmann::json::parse(std::string(body));

        // Required fields
        if (!nj.contains("weight_kg") || !nj.contains("weight_lb")) {
            cb(jsonError("weight_kg and weight_lb are required", drogon::k400BadRequest));
            return;
        }

        double weight_kg = nj["weight_kg"].get<double>();
        double weight_lb = nj["weight_lb"].get<double>();
        double impedance = nj.value("impedance", 0.0);

        if (weight_kg <= 0) {
            cb(jsonError("weight_kg must be positive", drogon::k400BadRequest));
            return;
        }

        if (weight_lb <= 0) {
            cb(jsonError("weight_lb must be positive", drogon::k400BadRequest));
            return;
        }

        auto result = subscriber_->processMeasurementWithResult(weight_kg, weight_lb, impedance);

        Json::Value resp;
        resp["status"] = "ok";
        resp["user"] = result.identified ? result.user_name : "unassigned";
        resp["confidence"] = result.confidence;
        resp["method"] = result.method;
        resp["identified"] = result.identified;
        cb(makeJsonResponse(resp));

    } catch (const nlohmann::json::exception& e) {
        cb(jsonError(std::string("invalid JSON: ") + e.what(), drogon::k400BadRequest));
    } catch (const std::exception& e) {
        cb(jsonError(e.what(), drogon::k500InternalServerError));
    }
}

#endif // BUILD_WITH_WEB
