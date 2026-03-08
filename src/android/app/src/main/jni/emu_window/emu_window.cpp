// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <android/native_window_jni.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <dlfcn.h>

#include "common/android/id_cache.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "input_common/drivers/android.h"
#include "input_common/drivers/touch_screen.h"
#include "input_common/drivers/virtual_amiibo.h"
#include "input_common/drivers/virtual_gamepad.h"
#include "input_common/main.h"
#include "jni/emu_window/emu_window.h"
#include "jni/native.h"

void EmuWindow_Android::OnSurfaceChanged(ANativeWindow* surface) {
    if (!surface) {
        LOG_INFO(Frontend, "EmuWindow_Android::OnSurfaceChanged received null surface");
        m_window_width = 0;
        m_window_height = 0;
        window_info.render_surface = nullptr;
        m_last_frame_rate_hint = -1.0f;
        m_pending_frame_rate_hint = -1.0f;
        m_pending_frame_rate_hint_votes = 0;
        m_smoothed_present_rate = 0.0f;
        m_last_frame_display_time = {};
        return;
    }

    m_window_width = ANativeWindow_getWidth(surface);
    m_window_height = ANativeWindow_getHeight(surface);

    // Ensures that we emulate with the correct aspect ratio.
    UpdateCurrentFramebufferLayout(m_window_width, m_window_height);

    window_info.render_surface = reinterpret_cast<void*>(surface);
    UpdateFrameRateHint();
}

void EmuWindow_Android::OnTouchPressed(int id, float x, float y) {
    const auto [touch_x, touch_y] = MapToTouchScreen(x, y);
    EmulationSession::GetInstance().GetInputSubsystem().GetTouchScreen()->TouchPressed(touch_x,
                                                                                       touch_y, id);
}

void EmuWindow_Android::OnTouchMoved(int id, float x, float y) {
    const auto [touch_x, touch_y] = MapToTouchScreen(x, y);
    EmulationSession::GetInstance().GetInputSubsystem().GetTouchScreen()->TouchMoved(touch_x,
                                                                                     touch_y, id);
}

void EmuWindow_Android::OnTouchReleased(int id) {
    EmulationSession::GetInstance().GetInputSubsystem().GetTouchScreen()->TouchReleased(id);
}

void EmuWindow_Android::OnFrameDisplayed() {
    UpdateObservedFrameRate();
    UpdateFrameRateHint();

    if (!m_first_frame) {
        Common::Android::RunJNIOnFiber<void>(
            [&](JNIEnv* env) { EmulationSession::GetInstance().OnEmulationStarted(); });
        m_first_frame = true;
    }
}

void EmuWindow_Android::UpdateObservedFrameRate() {
    const auto now = Clock::now();
    if (m_last_frame_display_time.time_since_epoch().count() != 0) {
        const auto frame_time = std::chrono::duration<float>(now - m_last_frame_display_time);
        const float seconds = frame_time.count();
        if (seconds > 0.0f) {
            const float instantaneous_rate = 1.0f / seconds;
            if (std::isfinite(instantaneous_rate) && instantaneous_rate >= 10.0f &&
                instantaneous_rate <= 240.0f) {
                constexpr float SmoothingFactor = 0.15f;
                if (m_smoothed_present_rate <= 0.0f) {
                    m_smoothed_present_rate = instantaneous_rate;
                } else {
                    m_smoothed_present_rate +=
                        (instantaneous_rate - m_smoothed_present_rate) * SmoothingFactor;
                }
            }
        }
    }
    m_last_frame_display_time = now;
}

float EmuWindow_Android::QuantizeFrameRateHint(float frame_rate) {
    if (!std::isfinite(frame_rate) || frame_rate < 20.0f) {
        return 0.0f;
    }

    constexpr std::array CandidateRates{30.0f, 45.0f, 60.0f, 90.0f, 120.0f};
    const auto best = std::min_element(CandidateRates.begin(), CandidateRates.end(),
                                       [frame_rate](float lhs, float rhs) {
                                           return std::fabs(frame_rate - lhs) <
                                                  std::fabs(frame_rate - rhs);
                                       });
    const float best_rate = *best;
    const float tolerance = std::max(best_rate * 0.18f, 5.0f);
    return std::fabs(frame_rate - best_rate) <= tolerance ? best_rate : 0.0f;
}

float EmuWindow_Android::GetFrameRateHint() const {
    if (!Settings::values.use_speed_limit.GetValue()) {
        return 0.0f;
    }

    const u16 speed_limit = Settings::SpeedLimit();
    if (speed_limit == 0) {
        return 0.0f;
    }

    if (speed_limit > 100) {
        return 0.0f;
    }

    constexpr float NominalFrameRate = 60.0f;
    const float desired_rate = NominalFrameRate * (static_cast<float>(speed_limit) / 100.0f);

    if (desired_rate < 20.0f) {
        return 0.0f;
    }

    if (m_last_frame_rate_hint > 0.0f && m_smoothed_present_rate > 0.0f) {
        const float observed_rate = m_smoothed_present_rate;
        switch (static_cast<int>(m_last_frame_rate_hint)) {
        case 30:
            if (observed_rate < 42.0f) {
                return 30.0f;
            }
            break;
        case 45:
            if (observed_rate >= 35.0f && observed_rate < 54.0f) {
                return 45.0f;
            }
            break;
        case 60:
            if (observed_rate >= 48.0f && observed_rate < 75.0f) {
                return 60.0f;
            }
            break;
        case 90:
            if (observed_rate >= 74.0f && observed_rate < 105.0f) {
                return 90.0f;
            }
            break;
        case 120:
            if (observed_rate >= 100.0f) {
                return 120.0f;
            }
            break;
        default:
            break;
        }
    }

    const float observed_hint = QuantizeFrameRateHint(m_smoothed_present_rate);
    if (observed_hint > 0.0f) {
        return observed_hint;
    }

    return QuantizeFrameRateHint(desired_rate);
}

void EmuWindow_Android::UpdateFrameRateHint() {
    auto* const surface = reinterpret_cast<ANativeWindow*>(window_info.render_surface);
    if (!surface) {
        return;
    }

    const float frame_rate_hint = GetFrameRateHint();
    if (std::fabs(frame_rate_hint - m_last_frame_rate_hint) < 0.01f) {
        m_pending_frame_rate_hint = frame_rate_hint;
        m_pending_frame_rate_hint_votes = 0;
        return;
    }

    if (frame_rate_hint == 0.0f) {
        m_pending_frame_rate_hint = frame_rate_hint;
        m_pending_frame_rate_hint_votes = 0;
    } else if (m_last_frame_rate_hint >= 0.0f) {
        if (std::fabs(frame_rate_hint - m_pending_frame_rate_hint) >= 0.01f) {
            m_pending_frame_rate_hint = frame_rate_hint;
            m_pending_frame_rate_hint_votes = 1;
            return;
        }

        ++m_pending_frame_rate_hint_votes;
        constexpr std::uint32_t StableVoteThreshold = 12;
        if (m_pending_frame_rate_hint_votes < StableVoteThreshold) {
            return;
        }
    }

    using SetFrameRateWithChangeStrategyFn =
        int32_t (*)(ANativeWindow*, float, int8_t, int8_t);
    static const auto set_frame_rate_with_change_strategy =
        reinterpret_cast<SetFrameRateWithChangeStrategyFn>(
            dlsym(RTLD_DEFAULT, "ANativeWindow_setFrameRateWithChangeStrategy"));

    if (!set_frame_rate_with_change_strategy) {
        return;
    }

    const auto result = set_frame_rate_with_change_strategy(
        surface, frame_rate_hint,
        static_cast<int8_t>(ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_DEFAULT),
        static_cast<int8_t>(ANATIVEWINDOW_CHANGE_FRAME_RATE_ONLY_IF_SEAMLESS));
    if (result != 0) {
        LOG_DEBUG(Frontend, "Failed to update Android surface frame rate hint: {}", result);
        return;
    }

    m_last_frame_rate_hint = frame_rate_hint;
    m_pending_frame_rate_hint = frame_rate_hint;
    m_pending_frame_rate_hint_votes = 0;
}

EmuWindow_Android::EmuWindow_Android(ANativeWindow* surface,
                                     std::shared_ptr<Common::DynamicLibrary> driver_library)
    : m_driver_library{driver_library} {
    LOG_INFO(Frontend, "initializing");

    if (!surface) {
        LOG_CRITICAL(Frontend, "surface is nullptr");
        return;
    }

    OnSurfaceChanged(surface);
    window_info.type = Core::Frontend::WindowSystemType::Android;
}
