#include "boards/syaw-7210/syaw_button.h"

#include <cassert>
#include <functional>

struct TestButton {
    std::function<void()> down, up;
    int clicks = 0, doubles = 0, holds = 0;
    void OnPressDown(std::function<void()> callback) { down = std::move(callback); }
    void OnPressUp(std::function<void()> callback) { up = std::move(callback); }
    void OnClick(std::function<void()>) { ++clicks; }
    void OnDoubleClick(std::function<void()>) { ++doubles; }
    void OnLongPress(std::function<void()>) { ++holds; }
};

struct TestApplication {
    DeviceState state = kDeviceStateIdle;
    int starts = 0, stops = 0;
    DeviceState GetDeviceState() const { return state; }
    void StartSyawListening() { ++starts; }
    void StopSyawListening() { ++stops; }
};

int main() {
    TestButton button;
    TestApplication app;
    bool connected = true;
    int provisioning = 0;
    BindSyawPttButton(button, app, [&]() { ++provisioning; }, [&]() { return connected; });
    assert(button.down && button.up);
    assert(button.clicks == 0 && button.doubles == 0 && button.holds == 0);
    button.down();
    button.up();
    assert(app.starts == 1 && app.stops == 1 && provisioning == 0);

    app.state = kDeviceStateStarting;
    connected = false;
    button.down();
    button.up();
    assert(provisioning == 1 && app.starts == 1);
    connected = true;
    button.down();
    assert(provisioning == 1 && app.starts == 1);

    for (auto state : {kDeviceStateUnknown, kDeviceStateWifiConfiguring,
                       kDeviceStateActivating, kDeviceStateAudioTesting,
                       kDeviceStateUpgrading, kDeviceStateFatalError}) {
        app.state = state;
        button.down();
        assert(provisioning == 1 && app.starts == 1);
    }
    for (auto state : {kDeviceStateListening, kDeviceStateSpeaking, kDeviceStateConnecting}) {
        app.state = state;
        button.down();
    }
    assert(app.starts == 4);
}
