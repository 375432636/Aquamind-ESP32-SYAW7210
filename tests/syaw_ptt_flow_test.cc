#include "application.h"

const std::string kStart =
    R"({"session_id":"test-session","type":"listen","state":"start","mode":"manual"})";
const std::string kStop =
    R"({"session_id":"test-session","type":"listen","state":"stop"})";
const std::string kAbort =
    R"({"session_id":"test-session","type":"abort","reason":"wake_word_detected"})";

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string scenario = argv[1];
    Application app;
    auto& protocol = *app.protocol_;

    if (scenario == "quick_release") {
        app.StartSyawListening();
        app.StopSyawListening();
        app.Drain();
        assert(protocol.opens == 0 && protocol.messages.empty());
        assert(!app.audio_service_.processing);
    } else if (scenario == "release_while_connecting") {
        protocol.while_opening = [&]() { app.StopSyawListening(); };
        app.StartSyawListening();
        app.Drain();
        assert(protocol.opens == 1 && protocol.messages.empty());
        assert(app.device_state_ == kDeviceStateIdle && !app.audio_service_.processing);
    } else if (scenario == "release_and_repress_while_connecting") {
        protocol.while_opening = [&]() {
            app.StopSyawListening();
            app.StartSyawListening();
        };
        app.StartSyawListening();
        app.Drain();
        assert(protocol.messages == std::vector<std::string>{kStart});
        app.StopSyawListening();
        app.Drain();
        assert((protocol.messages == std::vector<std::string>{kStart, kStop}));
    } else if (scenario == "connection_failure") {
        protocol.succeeds = false;
        app.StartSyawListening();
        app.Drain();
        assert(protocol.messages.empty() && app.device_state_ == kDeviceStateIdle);
        app.StopSyawListening();
        app.Drain();
        protocol.succeeds = true;
        app.StartSyawListening();
        app.Drain();
        assert(protocol.messages == std::vector<std::string>{kStart});
    } else if (scenario == "unsupported_states") {
        for (auto state : {kDeviceStateUnknown, kDeviceStateStarting,
                           kDeviceStateWifiConfiguring, kDeviceStateConnecting,
                           kDeviceStateActivating, kDeviceStateAudioTesting,
                           kDeviceStateUpgrading, kDeviceStateFatalError}) {
            app.device_state_ = state;
            app.StartSyawListening();
            app.Drain();
            app.StopSyawListening();
            app.Drain();
            assert(protocol.opens == 0 && protocol.messages.empty());
            assert(app.device_state_ == state);
        }
    } else if (scenario == "missing_protocol") {
        app.protocol_.reset();
        app.StartSyawListening();
        app.Drain();
        app.StopSyawListening();
        app.Drain();
        assert(!app.audio_service_.processing);
    } else if (scenario == "disconnect_before_release") {
        app.StartSyawListening();
        app.Drain();
        protocol.opened = false;
        app.SetDeviceState(kDeviceStateIdle);
        app.StopSyawListening();
        app.Drain();
        assert(protocol.messages == std::vector<std::string>{kStart});
        assert(!app.audio_service_.processing);
    } else {
        bool speaking = scenario == "auto_speaking" || scenario == "realtime_speaking";
        bool listening = scenario == "auto_listening" || scenario == "realtime_listening" ||
                         scenario == "listening_without_processor";
        if (speaking) app.device_state_ = kDeviceStateSpeaking;
        if (listening) app.device_state_ = kDeviceStateListening;
        protocol.opened = speaking || listening;
        if (scenario == "auto_listening" || scenario == "auto_speaking") {
            app.listening_mode_ = kListeningModeAutoStop;
        }
        app.audio_service_.processing =
            scenario == "realtime_speaking" || scenario == "auto_listening" ||
            scenario == "realtime_listening";
        app.StartSyawListening();
        app.Drain();
        std::vector<std::string> expected;
        if (speaking) expected.push_back(kAbort);
        expected.push_back(kStart);
        assert(protocol.messages == expected);
        assert(app.device_state_ == kDeviceStateListening);
        assert(app.audio_service_.processing && !app.audio_service_.wake_word);
        assert(app.audio_service_.decoder_resets > 0);
        assert(app.listening_mode_ == kListeningModeManualStop);
        if (scenario == "duplicate_edges") {
            app.StartSyawListening();
            app.Drain();
            assert(protocol.messages == expected);
        }
        app.StopSyawListening();
        app.Drain();
        app.StopSyawListening();
        app.Drain();
        expected.push_back(kStop);
        assert(protocol.messages == expected);
        assert(app.device_state_ == kDeviceStateIdle);
        assert(!app.audio_service_.processing && app.audio_service_.wake_word);
    }
    // PTT is temporary; it must not change the selected hands-free/AEC mode.
    assert(app.aec_mode_ == kAecOnDeviceSide);
}
