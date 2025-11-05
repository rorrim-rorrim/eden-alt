// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/core.h"
#include "core/hle/service/am/applet.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/am/window_system.h"
#include "core/hle/service/am/service/overlay_functions.h"
#include "core/hle/service/cmif_serialization.h"

namespace Service::AM {

IOverlayFunctions::IOverlayFunctions(Core::System& system_, std::shared_ptr<Applet> applet)
    : ServiceFramework{system_, "IOverlayFunctions"}, m_applet{std::move(applet)} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, D<&IOverlayFunctions::BeginToWatchShortHomeButtonMessage>, "BeginToWatchShortHomeButtonMessage"},
        {1, D<&IOverlayFunctions::EndToWatchShortHomeButtonMessage>, "EndToWatchShortHomeButtonMessage"},
        {2, D<&IOverlayFunctions::GetApplicationIdForLogo>, "GetApplicationIdForLogo"},
        {3, nullptr, "SetGpuTimeSliceBoost"},
        {4, D<&IOverlayFunctions::SetAutoSleepTimeAndDimmingTimeEnabled>, "SetAutoSleepTimeAndDimmingTimeEnabled"},
        {5, nullptr, "TerminateApplicationAndSetReason"},
        {6, nullptr, "SetScreenShotPermissionGlobally"},
        {10, nullptr, "StartShutdownSequenceForOverlay"},
        {11, nullptr, "StartRebootSequenceForOverlay"},
        {20, D<&IOverlayFunctions::SetHandlingHomeButtonShortPressedEnabled>, "SetHandlingHomeButtonShortPressedEnabled"},
        {21, nullptr, "SetHandlingTouchScreenInputEnabled"},
        {30, nullptr, "SetHealthWarningShowingState"},
        {31, nullptr, "IsHealthWarningRequired"},
        {40, nullptr, "GetApplicationNintendoLogo"},
        {41, nullptr, "GetApplicationStartupMovie"},
        {50, nullptr, "SetGpuTimeSliceBoostForApplication"},
        {60, nullptr, "Unknown60"},
        {90, nullptr, "SetRequiresGpuResourceUse"},
        {101, nullptr, "BeginToObserveHidInputForDevelop"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IOverlayFunctions::~IOverlayFunctions() = default;

Result IOverlayFunctions::BeginToWatchShortHomeButtonMessage() {
    LOG_DEBUG(Service_AM, "called");
    {
        std::scoped_lock lk{m_applet->lock};

        m_applet->overlay_in_foreground = true;
        m_applet->home_button_short_pressed_blocked = false;

        static constexpr s32 kOverlayForegroundZ = 100;
        m_applet->display_layer_manager.SetOverlayZIndex(kOverlayForegroundZ);

        LOG_INFO(Service_AM, "called, Overlay moved to FOREGROUND (z={}, overlay_in_foreground=true)", kOverlayForegroundZ);
    }

    if (auto* window_system = system.GetAppletManager().GetWindowSystem()) {
        window_system->RequestUpdate();
    }

    R_SUCCEED();
}

Result IOverlayFunctions::EndToWatchShortHomeButtonMessage() {
    LOG_DEBUG(Service_AM, "called");

    {
        std::scoped_lock lk{m_applet->lock};

        m_applet->overlay_in_foreground = false;
        m_applet->home_button_short_pressed_blocked = false;

        static constexpr s32 kOverlayBackgroundZ = -100;
        m_applet->display_layer_manager.SetOverlayZIndex(kOverlayBackgroundZ);

        LOG_INFO(Service_AM, "Overlay moved to BACKGROUND (z={}, overlay_in_foreground=false)", kOverlayBackgroundZ);
    }

    if (auto* window_system = system.GetAppletManager().GetWindowSystem()) {
        window_system->RequestUpdate();
    }

    R_SUCCEED();
}

Result IOverlayFunctions::GetApplicationIdForLogo(Out<u64> out_application_id) {
    LOG_DEBUG(Service_AM, "called");

    std::scoped_lock lk{m_applet->lock};
    u64 id = m_applet->screen_shot_identity.application_id;
    if (id == 0) {
        id = m_applet->program_id;
    }
    *out_application_id = id;
    R_SUCCEED();
}

Result IOverlayFunctions::SetAutoSleepTimeAndDimmingTimeEnabled(bool enabled) {
    LOG_WARNING(Service_AM, "called, enabled={}", enabled);
    std::scoped_lock lk{m_applet->lock};
    m_applet->auto_sleep_disabled = !enabled;
    R_SUCCEED();
}

Result IOverlayFunctions::SetHandlingHomeButtonShortPressedEnabled(bool enabled) {
    LOG_DEBUG(Service_AM, "called, enabled={}", enabled);
    std::scoped_lock lk{m_applet->lock};
    m_applet->home_button_short_pressed_blocked = false;
    R_SUCCEED();
}

} // namespace Service::AM
