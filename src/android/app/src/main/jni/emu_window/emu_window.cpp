// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <android/native_window_jni.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <dlfcn.h>

#include "common/android/id_cache.h"
#include "common/logging.h"
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
    UpdateFrameRateHint();

    if (!m_first_frame) {
        Common::Android::RunJNIOnFiber<void>(
            [&](JNIEnv* env) { EmulationSession::GetInstance().OnEmulationStarted(); });
        m_first_frame = true;
    }
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

    return desired_rate;
}

void EmuWindow_Android::UpdateFrameRateHint() {
    auto* const surface = reinterpret_cast<ANativeWindow*>(window_info.render_surface);
    if (!surface) {
        return;
    }

    const float frame_rate_hint = GetFrameRateHint();
    if (std::fabs(frame_rate_hint - m_last_frame_rate_hint) < 0.01f) {
        return;
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
