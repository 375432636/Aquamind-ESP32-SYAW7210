#include "application.h"

#if CONFIG_BOARD_TYPE_SYAW_7210

#include <esp_log.h>

void Application::StartSyawListening() {
    const auto token = syaw_ptt_request_.Begin();
    if (token == 0) return;

    Schedule([this, token]() {
        if (!syaw_ptt_request_.IsCurrent(token)) return;
        if (!protocol_ || (device_state_ != kDeviceStateIdle &&
                           device_state_ != kDeviceStateListening &&
                           device_state_ != kDeviceStateSpeaking)) {
            syaw_ptt_request_.CancelIfCurrent(token);
            return;
        }

        if (device_state_ == kDeviceStateSpeaking) {
            // Match aquamind-gimbal's wire-level interrupt marker without
            // changing Protocol::SendAbortSpeaking for every other board.
            AbortSpeaking(kAbortReasonWakeWordDetected);
        }
        if (!protocol_->IsAudioChannelOpened()) {
            SetDeviceState(kDeviceStateConnecting);
            const bool opened = protocol_->OpenAudioChannel();
            if (!opened || !syaw_ptt_request_.IsCurrent(token)) {
                syaw_ptt_request_.CancelIfCurrent(token);
                if (device_state_ == kDeviceStateConnecting) {
                    SetDeviceState(kDeviceStateIdle);
                }
                return;
            }
        }
        if (!syaw_ptt_request_.IsCurrent(token)) return;

        audio_service_.ResetDecoder();
        const bool already_processing = audio_service_.IsAudioProcessorRunning();
        const bool already_listening = device_state_ == kDeviceStateListening;
        SetListeningMode(kListeningModeManualStop);
        // Preserve the gimbal's manual-start notification when replacing an
        // auto/realtime turn. Entering Listening otherwise sends it implicitly.
        if (already_processing || already_listening) {
            protocol_->SendStartListening(kListeningModeManualStop);
        }
        if (already_listening && !already_processing) {
            audio_service_.EnableVoiceProcessing(true);
        }
        audio_service_.EnableWakeWordDetection(false);
        syaw_ptt_started_ = token;
        ESP_LOGI("SyawPTT", "GPIO0 pressed: manual capture started");
    });
}

void Application::StopSyawListening() {
    // Cancel synchronously: release can arrive while OpenAudioChannel blocks
    // the main loop. Do not use StopListening's early state filter here.
    const auto token = syaw_ptt_request_.Cancel();
    if (token == 0) return;

    Schedule([this, token]() {
        if (syaw_ptt_started_ != token) return;
        syaw_ptt_started_ = 0;
        if (protocol_ && device_state_ == kDeviceStateListening &&
            listening_mode_ == kListeningModeManualStop) {
            protocol_->SendStopListening();
            SetDeviceState(kDeviceStateIdle);
            ESP_LOGI("SyawPTT", "GPIO0 released: manual capture stopped");
        }
    });
}

#endif  // CONFIG_BOARD_TYPE_SYAW_7210
