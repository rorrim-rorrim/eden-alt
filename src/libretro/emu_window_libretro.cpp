// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "emu_window_libretro.h"
#include "common/logging/log.h"
#include <thread>

namespace Libretro {

// Static member initialization
std::thread::id LibretroGraphicsContext::main_thread_id{};

void LibretroGraphicsContext::SetMainThreadId() {
    main_thread_id = std::this_thread::get_id();
    LOG_INFO(Frontend, "LibretroGraphicsContext: Main thread ID set");
}

bool LibretroGraphicsContext::IsMainThread() {
    return std::this_thread::get_id() == main_thread_id;
}

// LibretroGraphicsContext implementation
LibretroGraphicsContext::LibretroGraphicsContext(retro_hw_get_proc_address_t get_proc_address)
    : hw_get_proc_address(get_proc_address) {
    LOG_DEBUG(Frontend, "LibretroGraphicsContext created with get_proc_address: {}",
              reinterpret_cast<void*>(get_proc_address));
}

LibretroGraphicsContext::~LibretroGraphicsContext() {
    LOG_DEBUG(Frontend, "LibretroGraphicsContext destroyed");
}

void LibretroGraphicsContext::SwapBuffers() {
    // In libretro, RetroArch handles buffer swapping via video_cb
    // We don't need to do anything here
}

void LibretroGraphicsContext::MakeCurrent() {
    // In libretro, the context is always current on the main thread
    // If we're not on the main thread, we can't make the context current
    if (!IsMainThread()) {
        LOG_WARNING(Frontend, "LibretroGraphicsContext: MakeCurrent called from non-main thread - OpenGL calls may fail");
    }
}

void LibretroGraphicsContext::DoneCurrent() {
    // Nothing to do - context stays current on main thread
}

// EmuWindowLibretro implementation
EmuWindowLibretro::EmuWindowLibretro() {
    LOG_INFO(Frontend, "EmuWindowLibretro: Initializing libretro emulator window");

    // Set up window system info for headless/libretro mode
    window_info.type = Core::Frontend::WindowSystemType::Headless;
    window_info.display_connection = nullptr;
    window_info.render_surface = nullptr;
    window_info.render_surface_scale = 1.0f;

    // Initialize default framebuffer layout
    Layout::FramebufferLayout layout;
    layout.width = fb_width;
    layout.height = fb_height;
    layout.screen.left = 0;
    layout.screen.top = 0;
    layout.screen.right = fb_width;
    layout.screen.bottom = fb_height;
    layout.is_srgb = false;

    NotifyFramebufferLayoutChanged(layout);
    NotifyClientAreaSizeChanged({fb_width, fb_height});

    LOG_INFO(Frontend, "EmuWindowLibretro: Initialized with {}x{} framebuffer", fb_width, fb_height);
}

EmuWindowLibretro::~EmuWindowLibretro() {
    LOG_INFO(Frontend, "EmuWindowLibretro: Shutting down");
}

std::unique_ptr<Core::Frontend::GraphicsContext> EmuWindowLibretro::CreateSharedContext() const {
    LOG_DEBUG(Frontend, "EmuWindowLibretro: CreateSharedContext called");

    if (hw_render_callback && hw_render_callback->get_proc_address) {
        LOG_DEBUG(Frontend, "EmuWindowLibretro: Creating LibretroGraphicsContext with HW proc address");
        return std::make_unique<LibretroGraphicsContext>(hw_render_callback->get_proc_address);
    }

    LOG_WARNING(Frontend, "EmuWindowLibretro: No HW render callback, returning null context");
    return std::make_unique<LibretroGraphicsContext>(nullptr);
}

void EmuWindowLibretro::OnFrameDisplayed() {
    LOG_TRACE(Frontend, "EmuWindowLibretro: OnFrameDisplayed");

    if (frame_callback) {
        frame_callback();
    }
}

void EmuWindowLibretro::SetHWRenderCallback(retro_hw_render_callback* hw_render) {
    hw_render_callback = hw_render;

    if (hw_render) {
        LOG_INFO(Frontend, "EmuWindowLibretro: HW render callback set - context_type: {}, "
                 "version: {}.{}, depth: {}, stencil: {}, bottom_left_origin: {}",
                 static_cast<int>(hw_render->context_type),
                 hw_render->version_major, hw_render->version_minor,
                 hw_render->depth, hw_render->stencil, hw_render->bottom_left_origin);
    } else {
        LOG_WARNING(Frontend, "EmuWindowLibretro: HW render callback cleared");
    }
}

void EmuWindowLibretro::SetContextReady(bool ready) {
    bool old_ready = context_ready.exchange(ready);

    if (old_ready != ready) {
        LOG_INFO(Frontend, "EmuWindowLibretro: Context ready state changed: {} -> {}",
                 old_ready, ready);
    }
}

uintptr_t EmuWindowLibretro::GetCurrentFramebuffer() const {
    if (!hw_render_callback || !hw_render_callback->get_current_framebuffer) {
        LOG_TRACE(Frontend, "EmuWindowLibretro: GetCurrentFramebuffer - no callback, returning 0");
        return 0;
    }

    uintptr_t fbo = hw_render_callback->get_current_framebuffer();
    LOG_TRACE(Frontend, "EmuWindowLibretro: GetCurrentFramebuffer returned FBO: {}", fbo);
    return fbo;
}

retro_hw_get_proc_address_t EmuWindowLibretro::GetProcAddress() const {
    if (!hw_render_callback) {
        LOG_TRACE(Frontend, "EmuWindowLibretro: GetProcAddress - no callback");
        return nullptr;
    }
    return hw_render_callback->get_proc_address;
}

void EmuWindowLibretro::SetFramebufferSize(unsigned width, unsigned height) {
    if (width == 0 || height == 0) {
        LOG_WARNING(Frontend, "EmuWindowLibretro: Invalid framebuffer size {}x{}, ignoring",
                    width, height);
        return;
    }

    fb_width = width;
    fb_height = height;

    // Update the framebuffer layout
    Layout::FramebufferLayout layout;
    layout.width = fb_width;
    layout.height = fb_height;
    layout.screen.left = 0;
    layout.screen.top = 0;
    layout.screen.right = fb_width;
    layout.screen.bottom = fb_height;
    layout.is_srgb = false;

    NotifyFramebufferLayoutChanged(layout);
    NotifyClientAreaSizeChanged({fb_width, fb_height});

    LOG_INFO(Frontend, "EmuWindowLibretro: Framebuffer size updated to {}x{}", fb_width, fb_height);
}

void EmuWindowLibretro::SetFrameCallback(std::function<void()> callback) {
    frame_callback = std::move(callback);
    LOG_DEBUG(Frontend, "EmuWindowLibretro: Frame callback set");
}

} // namespace Libretro
