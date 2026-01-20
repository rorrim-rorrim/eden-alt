// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <functional>
#include <atomic>
#include <thread>

#include "core/frontend/emu_window.h"
#include "core/frontend/graphics_context.h"

#include "libretro.h"

namespace Core {
class System;
}

namespace Libretro {

// Graphics context for libretro hardware rendering
// In libretro, the OpenGL context is managed by RetroArch and is always current
// on the main thread during retro_run(). We can't make it current on other threads.
class LibretroGraphicsContext : public Core::Frontend::GraphicsContext {
public:
    explicit LibretroGraphicsContext(retro_hw_get_proc_address_t get_proc_address);
    ~LibretroGraphicsContext() override;

    void SwapBuffers() override;
    void MakeCurrent() override;
    void DoneCurrent() override;

    retro_hw_get_proc_address_t GetProcAddress() const { return hw_get_proc_address; }

    // Check if we're on the main thread where context is valid
    static void SetMainThreadId();
    static bool IsMainThread();

private:
    retro_hw_get_proc_address_t hw_get_proc_address = nullptr;
    static std::thread::id main_thread_id;
};

// EmuWindow implementation for libretro
class EmuWindowLibretro : public Core::Frontend::EmuWindow {
public:
    explicit EmuWindowLibretro();
    ~EmuWindowLibretro() override;

    // EmuWindow interface
    std::unique_ptr<Core::Frontend::GraphicsContext> CreateSharedContext() const override;
    bool IsShown() const override { return true; }
    void OnFrameDisplayed() override;
    u32 GetPresentationFramebuffer() const override;

    // Libretro-specific methods
    void SetHWRenderCallback(retro_hw_render_callback* hw_render);
    void SetContextReady(bool ready);
    bool IsContextReady() const { return context_ready.load(); }

    uintptr_t GetCurrentFramebuffer() const;
    retro_hw_get_proc_address_t GetProcAddress() const;

    void SetFramebufferSize(unsigned width, unsigned height);
    void SetFrameCallback(std::function<void()> callback);

private:
    retro_hw_render_callback* hw_render_callback = nullptr;
    std::atomic<bool> context_ready{false};
    std::function<void()> frame_callback;

    unsigned fb_width = 1280;
    unsigned fb_height = 720;
};

} // namespace Libretro
