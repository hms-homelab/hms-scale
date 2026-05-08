#ifdef WITH_BLE

#include "ble/BleScaleClient.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace hms_colada {

static const sdbus::ServiceName BLUEZ{"org.bluez"};
static const sdbus::ObjectPath  ROOT{"/"};

// ── Constructor / Destructor ───────────────────────────────────────────────

BleScaleClient::BleScaleClient(const std::string& mac, MeasurementCallback cb)
    : mac_(mac), callback_(std::move(cb)) {
    // Normalise MAC to uppercase for BlueZ path construction
    std::transform(mac_.begin(), mac_.end(), mac_.begin(), ::toupper);
}

BleScaleClient::~BleScaleClient() {
    stop();
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void BleScaleClient::start() {
    running_ = true;
    thread_ = std::thread(&BleScaleClient::run, this);
    spdlog::info("BleScaleClient: started (MAC: {})", mac_);
}

void BleScaleClient::stop() {
    running_ = false;
    if (conn_) conn_->leaveEventLoop();
    if (thread_.joinable()) thread_.join();
    spdlog::info("BleScaleClient: stopped");
}

// ── Reconnect loop ─────────────────────────────────────────────────────────

void BleScaleClient::run() {
    while (running_) {
        if (!connected_) {
            conn_ = sdbus::createSystemBusConnection();
            conn_->enterEventLoopAsync();

            adapter_path_ = findAdapter();
            if (adapter_path_.empty()) {
                spdlog::error("BleScaleClient: no Bluetooth adapter found");
                conn_.reset();
                std::this_thread::sleep_for(std::chrono::seconds(60));
                continue;
            }

            if (findAndConnect()) {
                spdlog::info("BleScaleClient: connected, waiting for measurements");
                // Stay in loop monitoring connection state
                while (running_ && connected_) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    // Check device Connected property
                    try {
                        auto dev = sdbus::createProxy(*conn_, BLUEZ,
                                                       sdbus::ObjectPath{device_path_});
                        sdbus::Variant v = dev->getProperty("Connected")
                                              .onInterface("org.bluez.Device1");
                        if (!v.get<bool>()) {
                            spdlog::warn("BleScaleClient: device disconnected");
                            connected_ = false;
                        }
                    } catch (...) {
                        connected_ = false;
                    }
                }
                disconnect();
            }

            conn_->leaveEventLoop();
            conn_.reset();

            if (running_) {
                spdlog::info("BleScaleClient: reconnecting in 30s...");
                std::this_thread::sleep_for(std::chrono::seconds(30));
            }
        }
    }
}

// ── Adapter discovery ──────────────────────────────────────────────────────

std::string BleScaleClient::findAdapter() {
    try {
        auto proxy = sdbus::createProxy(*conn_, BLUEZ, ROOT);
        std::map<sdbus::ObjectPath,
                 std::map<std::string, std::map<std::string, sdbus::Variant>>> objects;
        proxy->callMethod("GetManagedObjects")
            .onInterface("org.freedesktop.DBus.ObjectManager")
            .storeResultsTo(objects);

        for (auto& [path, ifaces] : objects) {
            if (ifaces.count("org.bluez.Adapter1"))
                return std::string(path);
        }
    } catch (const std::exception& e) {
        spdlog::error("BleScaleClient: findAdapter error: {}", e.what());
    }
    return "";
}

// ── Device lookup by MAC ───────────────────────────────────────────────────

std::string BleScaleClient::findDeviceByMac() {
    // BlueZ device path: /org/bluez/hci0/dev_D0_4D_00_51_4F_8F
    std::string mac_path = mac_;
    std::replace(mac_path.begin(), mac_path.end(), ':', '_');
    std::string expected = adapter_path_ + "/dev_" + mac_path;

    try {
        auto proxy = sdbus::createProxy(*conn_, BLUEZ, ROOT);
        std::map<sdbus::ObjectPath,
                 std::map<std::string, std::map<std::string, sdbus::Variant>>> objects;
        proxy->callMethod("GetManagedObjects")
            .onInterface("org.freedesktop.DBus.ObjectManager")
            .storeResultsTo(objects);

        for (auto& [path, ifaces] : objects) {
            if (ifaces.count("org.bluez.Device1") &&
                std::string(path) == expected) {
                return expected;
            }
        }
    } catch (...) {}

    return "";
}

// ── Scan + connect ─────────────────────────────────────────────────────────

bool BleScaleClient::findAndConnect() {
    std::lock_guard<std::mutex> lk(connect_mutex_);

    // Try cached path first
    device_path_ = findDeviceByMac();

    if (device_path_.empty()) {
        spdlog::info("BleScaleClient: device not cached, starting discovery...");
        auto adapter = sdbus::createProxy(*conn_, BLUEZ, sdbus::ObjectPath{adapter_path_});
        try {
            adapter->callMethod("StartDiscovery").onInterface("org.bluez.Adapter1");
        } catch (const std::exception& e) {
            spdlog::error("BleScaleClient: StartDiscovery failed: {}", e.what());
            return false;
        }

        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (std::chrono::steady_clock::now() < deadline && device_path_.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            device_path_ = findDeviceByMac();
        }

        try { adapter->callMethod("StopDiscovery").onInterface("org.bluez.Adapter1"); }
        catch (...) {}

        if (device_path_.empty()) {
            spdlog::warn("BleScaleClient: scale not found during scan");
            return false;
        }
    }

    spdlog::info("BleScaleClient: found device at {}", device_path_);
    return connectDevice(device_path_);
}

bool BleScaleClient::connectDevice(const std::string& device_path) {
    auto device = sdbus::createProxy(*conn_, BLUEZ, sdbus::ObjectPath{device_path});

    bool ok = false;
    for (int attempt = 0; attempt < 3; attempt++) {
        try {
            device->callMethod("Connect").onInterface("org.bluez.Device1");
            ok = true;
            break;
        } catch (const std::exception& e) {
            spdlog::warn("BleScaleClient: connect attempt {}/3 failed: {}", attempt + 1, e.what());
            try { device->callMethod("Disconnect").onInterface("org.bluez.Device1"); } catch (...) {}
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
    if (!ok) return false;

    // Wait for ServicesResolved
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        try {
            sdbus::Variant v = device->getProperty("ServicesResolved")
                                  .onInterface("org.bluez.Device1");
            if (v.get<bool>()) {
                connected_ = true;
                spdlog::info("BleScaleClient: connected, services resolved");
                return discoverGatt() && enableNotifications();
            }
        } catch (...) {}
    }

    spdlog::error("BleScaleClient: services not resolved (timeout)");
    disconnect();
    return false;
}

// ── GATT ──────────────────────────────────────────────────────────────────

bool BleScaleClient::discoverGatt() {
    notify_char_path_.clear();

    try {
        auto proxy = sdbus::createProxy(*conn_, BLUEZ, ROOT);
        std::map<sdbus::ObjectPath,
                 std::map<std::string, std::map<std::string, sdbus::Variant>>> objects;
        proxy->callMethod("GetManagedObjects")
            .onInterface("org.freedesktop.DBus.ObjectManager")
            .storeResultsTo(objects);

        for (auto& [path, ifaces] : objects) {
            std::string p = std::string(path);
            if (p.find(device_path_) != 0) continue;
            if (!ifaces.count("org.bluez.GattCharacteristic1")) continue;

            auto& props = ifaces.at("org.bluez.GattCharacteristic1");
            if (props.count("UUID")) {
                std::string uuid = props.at("UUID").get<std::string>();
                if (uuid == NOTIFY_UUID) {
                    notify_char_path_ = p;
                    spdlog::info("BleScaleClient: notify char at {}", p);
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("BleScaleClient: GATT discovery error: {}", e.what());
        return false;
    }

    if (notify_char_path_.empty()) {
        spdlog::error("BleScaleClient: characteristic 0xFFF1 not found");
        return false;
    }
    return true;
}

bool BleScaleClient::enableNotifications() {
    try {
        notify_proxy_ = sdbus::createProxy(*conn_, BLUEZ,
                                            sdbus::ObjectPath{notify_char_path_});

        notify_proxy_->uponSignal("PropertiesChanged")
            .onInterface("org.freedesktop.DBus.Properties")
            .call([this](const std::string& iface,
                          const std::map<std::string, sdbus::Variant>& changed,
                          const std::vector<std::string>&) {
                if (iface == "org.bluez.GattCharacteristic1" &&
                    changed.count("Value")) {
                    auto value = changed.at("Value").get<std::vector<uint8_t>>();
                    onNotification(value);
                }
            });

        notify_proxy_->callMethod("StartNotify")
            .onInterface("org.bluez.GattCharacteristic1");

        spdlog::info("BleScaleClient: notifications enabled on 0xFFF1");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("BleScaleClient: StartNotify failed: {}", e.what());
        return false;
    }
}

void BleScaleClient::disconnect() {
    notify_proxy_.reset();
    if (!device_path_.empty() && conn_) {
        try {
            auto dev = sdbus::createProxy(*conn_, BLUEZ, sdbus::ObjectPath{device_path_});
            dev->callMethod("Disconnect").onInterface("org.bluez.Device1");
        } catch (...) {}
    }
    connected_     = false;
    notify_char_path_.clear();
}

// ── Packet parsing ─────────────────────────────────────────────────────────

void BleScaleClient::onNotification(const std::vector<uint8_t>& data) {
    parseMeasurement(data);
}

void BleScaleClient::parseMeasurement(const std::vector<uint8_t>& data) {
    if (data.size() != 22) {
        spdlog::debug("BleScaleClient: unexpected packet size {}", data.size());
        return;
    }

    if (data[0] != 0xA5 || data[1] != 0x02) {
        spdlog::debug("BleScaleClient: invalid header 0x{:02X} 0x{:02X}", data[0], data[1]);
        return;
    }

    bool stable          = (data[19] == 0x01);
    bool impedance_valid = (data[20] == 0x01);

    uint32_t weight_raw  = data[10] | (data[11] << 8) | (data[12] << 16);
    double weight_kg     = weight_raw / 1000.0;
    double weight_lb     = weight_kg * 2.20462;

    uint16_t impedance_raw = data[13] | (data[14] << 8);
    double impedance       = impedance_valid ? static_cast<double>(impedance_raw) : 0.0;

    spdlog::debug("BleScaleClient: raw packet — weight={:.3f}kg stable={} impedance={}Ω",
                  weight_kg, stable, impedance_raw);

    // Lock-on-stability: only fire for stable, meaningful, new measurements
    if (!stable)                                               return;
    if (weight_kg < MIN_WEIGHT_KG)                             return;
    if (std::fabs(weight_kg - last_locked_weight_) < LOCK_DELTA_KG) return;

    last_locked_weight_ = static_cast<float>(weight_kg);

    spdlog::info("BleScaleClient: measurement locked — {:.3f}kg ({:.1f}lb) impedance={}Ω",
                 weight_kg, weight_lb, impedance_valid ? impedance_raw : 0);

    if (callback_) {
        callback_(weight_kg, weight_lb, impedance);
    }
}

} // namespace hms_colada

#endif // WITH_BLE
