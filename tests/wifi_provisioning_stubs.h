#pragma once
#include <cassert>
#include <string>
#include <vector>

struct FakeNetif {
    bool alive = true;
    bool default_handler_attached = true;
};
inline FakeNetif station_netif;
inline bool has_saved_network = true;
inline bool station_connects = false;
inline int stops = 0;
inline int provisioning_starts = 0;
using esp_netif_t = FakeNetif;
inline esp_netif_t* esp_netif_get_handle_from_ifkey(const char*) { return &station_netif; }
inline int esp_wifi_clear_default_wifi_driver_and_handlers(esp_netif_t* netif) {
    assert(netif->alive);
    netif->default_handler_attached = false;
    return 0;
}
#define ESP_ERROR_CHECK(expression) assert((expression) == 0)

namespace Lang::Strings {
inline constexpr auto SCANNING_WIFI = "scanning";
inline constexpr auto CONNECT_TO = "connect";
inline constexpr auto CONNECTED_TO = "connected";
}
struct TestDisplay { void ShowNotification(const char*, int) {} };
struct Board {
    TestDisplay display;
    static Board& GetInstance() { static Board board; return board; }
    TestDisplay* GetDisplay() { return &display; }
};
struct SsidManager {
    static SsidManager& GetInstance() { static SsidManager manager; return manager; }
    std::vector<int> GetSsidList() { return has_saved_network ? std::vector<int>{1} : std::vector<int>{}; }
};
struct WifiStation {
    static WifiStation& GetInstance() { static WifiStation station; return station; }
    template<class Callback> void OnScanBegin(Callback) {}
    template<class Callback> void OnConnect(Callback) {}
    template<class Callback> void OnConnected(Callback) {}
    void Start() {}
    bool WaitForConnected(int) { return station_connects; }
    void Stop() {
        // esp-wifi-connect 2.6.2 destroys the netif but leaves its default
        // Wi-Fi event handler attached unless the caller clears it first.
        station_netif.alive = false;
        ++stops;
    }
};
class WifiBoard {
public:
    bool wifi_config_mode_ = false;
    TestDisplay display;
    TestDisplay* GetDisplay() { return &display; }
    void StartNetwork();
    void EnterWifiConfigMode() {
        // Starting APSTA emits STA_START. Its default handler must not use a
        // netif that was freed during the preceding station Stop().
        assert((!station_netif.default_handler_attached || station_netif.alive) &&
               "STA_START accessed freed network interface");
        ++provisioning_starts;
    }
};
