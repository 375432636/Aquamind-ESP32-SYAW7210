#pragma once

#include "boards/syaw-7210/syaw_ptt_request.h"
#include "device_state.h"
#include "protocol.h"

#include <cassert>
#include <deque>
#include <memory>

// Exercise the real board extension and protocol serialization. Only the
// hardware/audio service and the application event-loop shell are substituted.
class RecordingProtocol : public Protocol {
public:
    bool opened = false;
    bool succeeds = true;
    int opens = 0;
    std::function<void()> while_opening;
    std::vector<std::string> messages;

    RecordingProtocol() { session_id_ = "test-session"; }
    bool Start() override { return true; }
    bool OpenAudioChannel() override {
        ++opens;
        if (while_opening) while_opening();
        return opened = succeeds;
    }
    void CloseAudioChannel() override { opened = false; }
    bool IsAudioChannelOpened() const override { return opened; }
    bool SendAudio(std::unique_ptr<AudioStreamPacket>) override { return true; }
    bool SendText(const std::string& text) override {
        messages.push_back(text);
        return true;
    }
};

struct TestAudio {
    bool processing = false;
    bool wake_word = true;
    int decoder_resets = 0;
    bool IsAudioProcessorRunning() const { return processing; }
    void EnableVoiceProcessing(bool enabled) { processing = enabled; }
    void EnableWakeWordDetection(bool enabled) { wake_word = enabled; }
    void ResetDecoder() { ++decoder_resets; }
};

enum AecMode { kAecOff, kAecOnDeviceSide };

class Application {
public:
    std::deque<std::function<void()>> tasks;
    std::unique_ptr<RecordingProtocol> protocol_ = std::make_unique<RecordingProtocol>();
    TestAudio audio_service_;
    DeviceState device_state_ = kDeviceStateIdle;
    ListeningMode listening_mode_ = kListeningModeRealtime;
    AecMode aec_mode_ = kAecOnDeviceSide;
    SyawPttRequest syaw_ptt_request_;
    uint32_t syaw_ptt_started_ = 0;

    void StartSyawListening();
    void StopSyawListening();
    void Schedule(std::function<void()> callback) { tasks.push_back(std::move(callback)); }
    void Drain() {
        while (!tasks.empty()) {
            auto task = std::move(tasks.front());
            tasks.pop_front();
            task();
        }
    }
    void AbortSpeaking(AbortReason reason) { protocol_->SendAbortSpeaking(reason); }
    // Mirror the relevant production state transitions, including the no-op
    // when already listening and the implicit start when audio was stopped.
    void SetDeviceState(DeviceState state) {
        if (device_state_ == state) return;
        device_state_ = state;
        if (state == kDeviceStateIdle) {
            audio_service_.EnableVoiceProcessing(false);
            audio_service_.EnableWakeWordDetection(true);
        } else if (state == kDeviceStateListening && !audio_service_.processing) {
            protocol_->SendStartListening(listening_mode_);
            audio_service_.EnableVoiceProcessing(true);
            audio_service_.EnableWakeWordDetection(false);
        }
    }
    void SetListeningMode(ListeningMode mode) {
        listening_mode_ = mode;
        SetDeviceState(kDeviceStateListening);
    }
};
