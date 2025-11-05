// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/service/am/am_results.h"
#include "core/hle/service/am/applet.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/am/event_observer.h"
#include "core/hle/service/am/window_system.h"

namespace Service::AM {

WindowSystem::WindowSystem(Core::System& system) : m_system(system) {}

WindowSystem::~WindowSystem() {
    m_system.GetAppletManager().SetWindowSystem(nullptr);
}

void WindowSystem::SetEventObserver(EventObserver* observer) {
    m_event_observer = observer;
    m_system.GetAppletManager().SetWindowSystem(this);
}

void WindowSystem::RequestUpdate() {
    if (m_event_observer) {
        m_event_observer->RequestUpdate();
    }
}

void WindowSystem::Update() {
    std::scoped_lock lk{m_lock};

    LOG_DEBUG(Service_AM, "WindowSystem::Update called - home_menu={} application={} overlay={}",
              m_home_menu != nullptr, m_application != nullptr, m_overlay_display != nullptr);

    // Loop through all applets and remove terminated applets.
    this->PruneTerminatedAppletsLocked();

    // If the home menu is being locked into the foreground, handle that.
    if (this->LockHomeMenuIntoForegroundLocked()) {
        return;
    }

    bool overlay_blocks_input = false;
    if (m_overlay_display) {
        std::scoped_lock lk_overlay{m_overlay_display->lock};
        overlay_blocks_input = m_overlay_display->overlay_in_foreground;
    }

    // Recursively update each applet root.
    this->UpdateAppletStateLocked(m_home_menu, m_foreground_requested_applet == m_home_menu, overlay_blocks_input);
    this->UpdateAppletStateLocked(m_application, m_foreground_requested_applet == m_application, overlay_blocks_input);
    this->UpdateAppletStateLocked(m_overlay_display, true, false); // overlay is always updated, never blocked
}

void WindowSystem::TrackApplet(std::shared_ptr<Applet> applet, bool is_application) {
    std::scoped_lock lk{m_lock};

    if (applet->applet_id == AppletId::QLaunch) {
        ASSERT(m_home_menu == nullptr);
        m_home_menu = applet.get();
    } else if (applet->applet_id == AppletId::OverlayDisplay) {
        // Track overlay display applet
        m_overlay_display = applet.get();
    } else if (is_application) {
        ASSERT(m_application == nullptr);
        m_application = applet.get();
    }

    m_event_observer->TrackAppletProcess(*applet);
    m_applets.emplace(applet->aruid.pid, std::move(applet));
}

std::shared_ptr<Applet> WindowSystem::GetByAppletResourceUserId(u64 aruid) {
    std::scoped_lock lk{m_lock};

    const auto it = m_applets.find(aruid);
    if (it == m_applets.end()) {
        return nullptr;
    }

    return it->second;
}

std::shared_ptr<Applet> WindowSystem::GetMainApplet() {
    std::scoped_lock lk{m_lock};

    if (m_application) {
        return m_applets.at(m_application->aruid.pid);
    }

    return nullptr;
}

void WindowSystem::RequestHomeMenuToGetForeground() {
    {
        std::scoped_lock lk{m_lock};
        m_foreground_requested_applet = m_home_menu;
    }

    m_event_observer->RequestUpdate();
}

void WindowSystem::RequestApplicationToGetForeground() {
    {
        std::scoped_lock lk{m_lock};
        m_foreground_requested_applet = m_application;
    }

    m_event_observer->RequestUpdate();
}

void WindowSystem::RequestLockHomeMenuIntoForeground() {
    {
        std::scoped_lock lk{m_lock};
        m_home_menu_foreground_locked = true;
    }

    m_event_observer->RequestUpdate();
}

void WindowSystem::RequestUnlockHomeMenuIntoForeground() {
    {
        std::scoped_lock lk{m_lock};
        m_home_menu_foreground_locked = false;
    }

    m_event_observer->RequestUpdate();
}

void WindowSystem::RequestAppletVisibilityState(Applet& applet, bool visible) {
    {
        std::scoped_lock lk{applet.lock};
        applet.window_visible = visible;
    }

    m_event_observer->RequestUpdate();
}

void WindowSystem::OnOperationModeChanged() {
    std::scoped_lock lk{m_lock};

    for (const auto& [aruid, applet] : m_applets) {
        std::scoped_lock lk2{applet->lock};
        applet->lifecycle_manager.OnOperationAndPerformanceModeChanged();
    }
}

void WindowSystem::OnExitRequested() {
    std::scoped_lock lk{m_lock};

    for (const auto& [aruid, applet] : m_applets) {
        std::scoped_lock lk2{applet->lock};
        applet->lifecycle_manager.RequestExit();
    }
}

void WindowSystem::OnHomeButtonPressed(ButtonPressDuration type) {
    std::scoped_lock lk{m_lock};

    LOG_INFO(Service_AM, "WindowSystem::OnHomeButtonPressed - type={} overlay={} home_menu={} application={} foreground={}",
             type == ButtonPressDuration::ShortPressing ? "Short" : "Long",
             m_overlay_display != nullptr, m_home_menu != nullptr, m_application != nullptr,
             m_foreground_requested_applet == m_home_menu ? "home_menu" :
             m_foreground_requested_applet == m_application ? "application" : "none");

    // Priority 1: Check overlay first (works everywhere, even in qlaunch)
    // Long press always goes to overlay for toggling
    // Short press only when overlay is already open (to close it)
    if (m_overlay_display) {
        std::scoped_lock lk3{m_overlay_display->lock};
        const bool overlay_should_handle = !m_overlay_display->home_button_short_pressed_blocked;

        if (overlay_should_handle) {
            const bool overlay_in_fg = m_overlay_display->overlay_in_foreground;

            if (type == ButtonPressDuration::LongPressing ||
                (type == ButtonPressDuration::ShortPressing && overlay_in_fg)) {

                LOG_INFO(Service_AM, "Sending {} press to overlay (foreground={})",
                         type == ButtonPressDuration::ShortPressing ? "short" : "long",
                         overlay_in_fg);

                if (type == ButtonPressDuration::ShortPressing) {
                    m_overlay_display->lifecycle_manager.PushUnorderedMessage(
                        AppletMessage::DetectShortPressingHomeButton);
                } else {
                    m_overlay_display->lifecycle_manager.PushUnorderedMessage(
                        AppletMessage::DetectLongPressingHomeButton);
                }
                return;
            }
        }
    }

    if (m_home_menu) {
        std::scoped_lock lk2{m_home_menu->lock};
        LOG_DEBUG(Service_AM, "called, Sending home button to home menu");
        if (type == ButtonPressDuration::ShortPressing) {
            m_home_menu->lifecycle_manager.PushUnorderedMessage(
                AppletMessage::DetectShortPressingHomeButton);
        } else if (type == ButtonPressDuration::LongPressing) {
            m_home_menu->lifecycle_manager.PushUnorderedMessage(
                AppletMessage::DetectLongPressingHomeButton);
        }
        return;
    }

    LOG_DEBUG(Service_AM, "called, No target for home button press");
}

void WindowSystem::PruneTerminatedAppletsLocked() {
    for (auto it = m_applets.begin(); it != m_applets.end(); /* ... */) {
        const auto& [aruid, applet] = *it;

        std::scoped_lock lk{applet->lock};

        if (!applet->process->IsTerminated()) {
            // Not terminated.
            it = std::next(it);
            continue;
        }

        // Terminated, so ensure all child applets are terminated.
        if (!applet->child_applets.empty()) {
            this->TerminateChildAppletsLocked(applet.get());

            // Not ready to unlink until all child applets are terminated.
            it = std::next(it);
            continue;
        }

        // Erase from caller applet's list of children.
        if (auto caller_applet = applet->caller_applet.lock(); caller_applet) {
            std::scoped_lock lk2{caller_applet->lock};
            std::erase(caller_applet->child_applets, applet);
            applet->caller_applet.reset();

            // We don't need to update the activity state of the caller applet yet.
            // It will be recalculated once we fall out of the termination handling path.
        }

        // If this applet was foreground, it no longer is.
        if (applet.get() == m_foreground_requested_applet) {
            m_foreground_requested_applet = nullptr;
        }

        // If this was the home menu, we should clean up.
        if (applet.get() == m_home_menu) {
            m_home_menu = nullptr;
            m_foreground_requested_applet = m_application;
        }

        // If this was the application, we should try to switch to the home menu.
        if (applet.get() == m_application) {
            m_application = nullptr;
            m_foreground_requested_applet = m_home_menu;

            // If we have a home menu, send it the application exited message.
            if (m_home_menu) {
                m_home_menu->lifecycle_manager.PushUnorderedMessage(
                    AppletMessage::ApplicationExited);
            }
        }

        // If this was the overlay display, clear the pointer.
        if (applet.get() == m_overlay_display) {
            m_overlay_display = nullptr;
        }

        // Finalize applet.
        applet->OnProcessTerminatedLocked();

        // Request update to ensure quiescence.
        m_event_observer->RequestUpdate();

        // Unlink and advance.
        it = m_applets.erase(it);
    }

    // If the last applet has exited, exit the system.
    if (m_applets.empty()) {
        m_system.Exit();
    }
}

bool WindowSystem::LockHomeMenuIntoForegroundLocked() {
    // If the home menu is not locked into foreground, then there's nothing to do.
    if (m_home_menu == nullptr || !m_home_menu_foreground_locked) {
        m_home_menu_foreground_locked = false;
        return false;
    }

    // Terminate any direct child applets of the home menu.
    std::scoped_lock lk{m_home_menu->lock};

    this->TerminateChildAppletsLocked(m_home_menu);

    // When there are zero child applets left, we can proceed with the update.
    if (m_home_menu->child_applets.empty()) {
        m_home_menu->window_visible = true;
        m_foreground_requested_applet = m_home_menu;
        return false;
    }

    return true;
}

void WindowSystem::TerminateChildAppletsLocked(Applet* applet) {
    auto child_applets = applet->child_applets;

    applet->lock.unlock();
    for (const auto& child_applet : child_applets) {
        std::scoped_lock lk{child_applet->lock};
        child_applet->process->Terminate();
        child_applet->terminate_result = AM::ResultLibraryAppletTerminated;
    }
    applet->lock.lock();
}

void WindowSystem::UpdateAppletStateLocked(Applet* applet, bool is_foreground, bool overlay_blocking) {
    // With no applet, we don't have anything to do.
    if (!applet) {
        return;
    }

    std::scoped_lock lk{applet->lock};

    const bool inherited_foreground = applet->is_process_running && is_foreground;
    const auto visible_state =
        inherited_foreground ? ActivityState::ForegroundVisible : ActivityState::BackgroundVisible;
    const auto obscured_state = inherited_foreground ? ActivityState::ForegroundObscured
                                                     : ActivityState::BackgroundObscured;

    const bool has_obscuring_child_applets = [&] {
        for (const auto& child_applet : applet->child_applets) {
            std::scoped_lock lk2{child_applet->lock};
            const auto mode = child_applet->library_applet_mode;
            if (child_applet->is_process_running && child_applet->window_visible &&
                (mode == LibraryAppletMode::AllForeground ||
                 mode == LibraryAppletMode::AllForegroundInitiallyHidden)) {
                return true;
            }
        }

        return false;
    }();

    // Update visibility state.
    // Overlay applets should always be visible when window_visible is true, regardless of foreground state
    const bool should_be_visible = (applet->applet_id == AppletId::OverlayDisplay)
                                     ? applet->window_visible
                                     : (is_foreground && applet->window_visible);
    applet->display_layer_manager.SetWindowVisibility(should_be_visible);


    const bool should_be_interactible = (applet->applet_id == AppletId::OverlayDisplay)
                                          ? applet->overlay_in_foreground
                                          : (is_foreground && applet->window_visible && !overlay_blocking);

    if (applet->applet_id == AppletId::OverlayDisplay || applet->applet_id == AppletId::Application) {
        LOG_DEBUG(Service_AM, "UpdateAppletStateLocked: applet={} overlay_in_foreground={} is_foreground={} window_visible={} overlay_blocking={} should_be_interactible={}",
                 static_cast<u32>(applet->applet_id), applet->overlay_in_foreground, is_foreground, applet->window_visible, overlay_blocking, should_be_interactible);
    }

    applet->SetInteractibleLocked(should_be_interactible);

    // Update focus state and suspension.
    const bool is_obscured = has_obscuring_child_applets || !applet->window_visible;
    const auto state = applet->lifecycle_manager.GetActivityState();

    if (is_obscured && state != obscured_state) {
        // Set obscured state.
        applet->lifecycle_manager.SetActivityState(obscured_state);
        applet->UpdateSuspensionStateLocked(true);
    } else if (!is_obscured && state != visible_state) {
        // Set visible state.
        applet->lifecycle_manager.SetActivityState(visible_state);
        applet->UpdateSuspensionStateLocked(true);
    }

    // Recurse into child applets.
    for (const auto& child_applet : applet->child_applets) {
        this->UpdateAppletStateLocked(child_applet.get(), is_foreground, overlay_blocking);
    }
}

} // namespace Service::AM
