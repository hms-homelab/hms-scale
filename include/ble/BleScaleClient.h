#pragma once

#ifdef WITH_BLE

#include <sdbus-c++/sdbus-c++.h>
#include <functional>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

namespace hms_colada {

/**
 * BleScaleClient — direct BLE connection to Etekcity scale via BlueZ D-Bus (sdbus-c++)
 *
 * Scans by MAC address, connects to service 0xFFF0, subscribes to characteristic
 * 0xFFF1 notifications, and parses the 22-byte Etekcity packet. Runs a persistent
 * reconnect loop so measurements flow whenever someone steps on the scale.
 *
 * Packet format (22 bytes):
 *   [0-1]  header: 0xA5 0x02
 *   [10-12] weight (LE, milligrams → kg)
 *   [13-14] impedance (LE, ohms)
 *   [18]   battery %
 *   [19]   stability flag (0x01 = stable)
 *   [20]   impedance valid flag (0x01 = valid)
 */
class BleScaleClient {
public:
    using MeasurementCallback = std::function<void(double weight_kg, double weight_lb, double impedance_ohm)>;

    BleScaleClient(const std::string& mac, MeasurementCallback cb);
    ~BleScaleClient();

    void start();
    void stop();
    bool isConnected() const { return connected_.load(); }

private:
    void run();
    bool findAndConnect();
    std::string findAdapter();
    std::string findDeviceByMac();
    bool connectDevice(const std::string& device_path);
    bool discoverGatt();
    bool enableNotifications();
    void disconnect();

    void onNotification(const std::vector<uint8_t>& data);
    void parseMeasurement(const std::vector<uint8_t>& data);

    std::string mac_;
    MeasurementCallback callback_;

    std::unique_ptr<sdbus::IConnection> conn_;
    std::string adapter_path_;
    std::string device_path_;
    std::string notify_char_path_;
    std::unique_ptr<sdbus::IProxy> notify_proxy_;

    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::thread thread_;
    std::mutex connect_mutex_;

    float last_locked_weight_ = 0.0f;

    static constexpr const char* NOTIFY_UUID = "0000fff1-0000-1000-8000-00805f9b34fb";
    static constexpr float MIN_WEIGHT_KG     = 5.0f;
    static constexpr float LOCK_DELTA_KG     = 0.5f;
};

} // namespace hms_colada

#endif // WITH_BLE
