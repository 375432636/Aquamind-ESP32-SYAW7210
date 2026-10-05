#pragma once

#include "device_state.h"

// GPIO0 has one gesture owner. Do not also register click/double-click handlers:
// the button library emits those after release and would toggle chat after PTT.
template <typename ButtonType, typename ApplicationType,
          typename EnterWifiConfig, typename IsWifiConnected>
void BindSyawPttButton(ButtonType& button, ApplicationType& app,
                       EnterWifiConfig enter_wifi_config, IsWifiConnected wifi_connected) {
    button.OnPressDown([&app, enter_wifi_config, wifi_connected]() {
        const auto state = app.GetDeviceState();
        if (state == kDeviceStateStarting) {
            if (!wifi_connected()) enter_wifi_config();
            return;
        }
        if (state == kDeviceStateIdle || state == kDeviceStateListening ||
            state == kDeviceStateSpeaking || state == kDeviceStateConnecting) {
            // A new press during connection is rechecked on the main loop.
            app.StartSyawListening();
        }
    });
    button.OnPressUp([&app]() { app.StopSyawListening(); });
}
