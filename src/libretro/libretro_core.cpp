// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <atomic>
#include <cstdarg>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "common/fs/path_util.h"

#include <glad/glad.h>

#include "libretro.h"
#include "libretro_vfs.h"
#include "emu_window_libretro.h"

#include "common/common_types.h"
#include "common/detached_tasks.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/settings.h"
#include "common/string_util.h"

#include "core/core.h"
#include "core/cpu_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/loader.h"
#include "core/perf_stats.h"

#include "video_core/renderer_base.h"
#include "video_core/gpu.h"
#include "video_core/rasterizer_interface.h"

#include "hid_core/hid_core.h"

#include "audio_core/audio_core.h"

#include "input_common/main.h"
#include "input_common/drivers/virtual_gamepad.h"

// Global state
namespace {

// Libretro callbacks
retro_environment_t environ_cb = nullptr;
retro_video_refresh_t video_cb = nullptr;
retro_audio_sample_t audio_sample_cb = nullptr;
retro_audio_sample_batch_t audio_batch_cb = nullptr;
retro_input_poll_t input_poll_cb = nullptr;
retro_input_state_t input_state_cb = nullptr;
retro_log_printf_t log_cb = nullptr;

// Hardware rendering
retro_hw_render_callback hw_render = {};
bool hw_context_ready = false;

// Core state
std::unique_ptr<Core::System> emu_system;
std::unique_ptr<Libretro::EmuWindowLibretro> emu_window;
std::unique_ptr<Common::DetachedTasks> detached_tasks;
std::unique_ptr<InputCommon::InputSubsystem> input_subsystem;

std::string game_path;
std::string system_directory;
std::string save_directory;

std::atomic<bool> is_running{false};
std::atomic<bool> game_loaded{false};
std::atomic<bool> is_initialized{false};
std::atomic<uint64_t> frame_count{0};
std::mutex emu_mutex;

// Audio buffer
std::vector<int16_t> audio_buffer;
constexpr size_t AUDIO_BUFFER_SIZE = 2048 * 2; // Stereo samples

// Screen dimensions
constexpr unsigned SCREEN_WIDTH = 1280;
constexpr unsigned SCREEN_HEIGHT = 720;
constexpr double FPS = 60.0;
constexpr double SAMPLE_RATE = 48000.0;

// Input mapping: libretro -> Switch
struct InputMapping {
    unsigned retro_id;
    int switch_button;
};

// Log callback wrapper
void LibretroLog(retro_log_level level, const char* fmt, ...) {
    if (!log_cb) return;

    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    log_cb(level, "%s", buffer);
}

// Helper to get core option value
static const char* GetCoreOptionValue(const char* key) {
    if (!environ_cb) return nullptr;

    struct retro_variable var = { key, nullptr };
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        return var.value;
    }
    return nullptr;
}

// Read and apply core options to Settings
void ApplyCoreOptions() {
    LOG_INFO(Frontend, "Libretro: Reading core options");

    // Resolution Scale
    if (const char* value = GetCoreOptionValue("eden_resolution_scale")) {
        int scale = 1;
        if (sscanf(value, "%dx", &scale) == 1) {
            Settings::ResolutionSetup setup = Settings::ResolutionSetup::Res1X;
            switch (scale) {
                case 1: setup = Settings::ResolutionSetup::Res1X; break;
                case 2: setup = Settings::ResolutionSetup::Res2X; break;
                case 3: setup = Settings::ResolutionSetup::Res3X; break;
                case 4: setup = Settings::ResolutionSetup::Res4X; break;
                case 5: setup = Settings::ResolutionSetup::Res5X; break;
                case 6: setup = Settings::ResolutionSetup::Res6X; break;
            }
            Settings::values.resolution_setup.SetValue(setup);
            LOG_INFO(Frontend, "Resolution Scale: {}x", scale);
        }
    }

    // GPU Accuracy
    if (const char* value = GetCoreOptionValue("eden_gpu_accuracy")) {
        Settings::GpuAccuracy accuracy = Settings::GpuAccuracy::Medium;
        if (strcmp(value, "Low") == 0) accuracy = Settings::GpuAccuracy::Low;
        else if (strcmp(value, "Medium") == 0) accuracy = Settings::GpuAccuracy::Medium;
        else if (strcmp(value, "High") == 0) accuracy = Settings::GpuAccuracy::High;
        Settings::values.gpu_accuracy.SetValue(accuracy);
        LOG_INFO(Frontend, "GPU Accuracy: {}", value);
    }

    // Scaling Filter
    if (const char* value = GetCoreOptionValue("eden_scaling_filter")) {
        Settings::ScalingFilter filter = Settings::ScalingFilter::Bilinear;
        if (strcmp(value, "NearestNeighbor") == 0) filter = Settings::ScalingFilter::NearestNeighbor;
        else if (strcmp(value, "Bilinear") == 0) filter = Settings::ScalingFilter::Bilinear;
        else if (strcmp(value, "Bicubic") == 0) filter = Settings::ScalingFilter::Bicubic;
        else if (strcmp(value, "Gaussian") == 0) filter = Settings::ScalingFilter::Gaussian;
        else if (strcmp(value, "ScaleForce") == 0) filter = Settings::ScalingFilter::ScaleForce;
        else if (strcmp(value, "Fsr") == 0) filter = Settings::ScalingFilter::Fsr;
        Settings::values.scaling_filter.SetValue(filter);
        LOG_INFO(Frontend, "Scaling Filter: {}", value);
    }

    // Anti-Aliasing
    if (const char* value = GetCoreOptionValue("eden_anti_aliasing")) {
        Settings::AntiAliasing aa = Settings::AntiAliasing::None;
        if (strcmp(value, "None") == 0) aa = Settings::AntiAliasing::None;
        else if (strcmp(value, "Fxaa") == 0) aa = Settings::AntiAliasing::Fxaa;
        else if (strcmp(value, "Smaa") == 0) aa = Settings::AntiAliasing::Smaa;
        Settings::values.anti_aliasing.SetValue(aa);
        LOG_INFO(Frontend, "Anti-Aliasing: {}", value);
    }

    // Anisotropic Filtering
    if (const char* value = GetCoreOptionValue("eden_anisotropic_filtering")) {
        Settings::AnisotropyMode aniso = Settings::AnisotropyMode::Automatic;
        if (strcmp(value, "Automatic") == 0) aniso = Settings::AnisotropyMode::Automatic;
        else if (strcmp(value, "Default") == 0) aniso = Settings::AnisotropyMode::Default;
        else if (strcmp(value, "X2") == 0) aniso = Settings::AnisotropyMode::X2;
        else if (strcmp(value, "X4") == 0) aniso = Settings::AnisotropyMode::X4;
        else if (strcmp(value, "X8") == 0) aniso = Settings::AnisotropyMode::X8;
        else if (strcmp(value, "X16") == 0) aniso = Settings::AnisotropyMode::X16;
        Settings::values.max_anisotropy.SetValue(aniso);
        LOG_INFO(Frontend, "Anisotropic Filtering: {}", value);
    }

    // ASTC Decode
    if (const char* value = GetCoreOptionValue("eden_astc_decode")) {
        Settings::AstcDecodeMode decode = Settings::AstcDecodeMode::Gpu;
        if (strcmp(value, "Cpu") == 0) decode = Settings::AstcDecodeMode::Cpu;
        else if (strcmp(value, "Gpu") == 0) decode = Settings::AstcDecodeMode::Gpu;
        else if (strcmp(value, "CpuAsynchronous") == 0) decode = Settings::AstcDecodeMode::CpuAsynchronous;
        Settings::values.accelerate_astc.SetValue(decode);
        LOG_INFO(Frontend, "ASTC Decode: {}", value);
    }

    // Docked Mode
    if (const char* value = GetCoreOptionValue("eden_docked_mode")) {
        Settings::ConsoleMode mode = Settings::ConsoleMode::Docked;
        if (strcmp(value, "Docked") == 0) mode = Settings::ConsoleMode::Docked;
        else if (strcmp(value, "Handheld") == 0) mode = Settings::ConsoleMode::Handheld;
        Settings::values.use_docked_mode.SetValue(mode);
        LOG_INFO(Frontend, "Console Mode: {}", value);
    }

    // Language
    if (const char* value = GetCoreOptionValue("eden_language")) {
        Settings::Language lang = Settings::Language::EnglishAmerican;
        if (strcmp(value, "Japanese") == 0) lang = Settings::Language::Japanese;
        else if (strcmp(value, "EnglishAmerican") == 0) lang = Settings::Language::EnglishAmerican;
        else if (strcmp(value, "French") == 0) lang = Settings::Language::French;
        else if (strcmp(value, "German") == 0) lang = Settings::Language::German;
        else if (strcmp(value, "Italian") == 0) lang = Settings::Language::Italian;
        else if (strcmp(value, "Spanish") == 0) lang = Settings::Language::Spanish;
        else if (strcmp(value, "Chinese") == 0) lang = Settings::Language::Chinese;
        else if (strcmp(value, "Korean") == 0) lang = Settings::Language::Korean;
        else if (strcmp(value, "Dutch") == 0) lang = Settings::Language::Dutch;
        else if (strcmp(value, "Portuguese") == 0) lang = Settings::Language::Portuguese;
        else if (strcmp(value, "Russian") == 0) lang = Settings::Language::Russian;
        else if (strcmp(value, "EnglishBritish") == 0) lang = Settings::Language::EnglishBritish;
        Settings::values.language_index.SetValue(lang);
        LOG_INFO(Frontend, "Language: {}", value);
    }

    // Region
    if (const char* value = GetCoreOptionValue("eden_region")) {
        Settings::Region region = Settings::Region::Usa;
        if (strcmp(value, "Japan") == 0) region = Settings::Region::Japan;
        else if (strcmp(value, "Usa") == 0) region = Settings::Region::Usa;
        else if (strcmp(value, "Europe") == 0) region = Settings::Region::Europe;
        else if (strcmp(value, "Australia") == 0) region = Settings::Region::Australia;
        else if (strcmp(value, "China") == 0) region = Settings::Region::China;
        else if (strcmp(value, "Korea") == 0) region = Settings::Region::Korea;
        else if (strcmp(value, "Taiwan") == 0) region = Settings::Region::Taiwan;
        Settings::values.region_index.SetValue(region);
        LOG_INFO(Frontend, "Region: {}", value);
    }

    // Multicore CPU
    if (const char* value = GetCoreOptionValue("eden_use_multicore")) {
        bool multicore = (strcmp(value, "enabled") == 0);
        Settings::values.use_multi_core.SetValue(multicore);
        LOG_INFO(Frontend, "Multicore CPU: {}", multicore);
    }

    // CPU Accuracy
    if (const char* value = GetCoreOptionValue("eden_cpu_accuracy")) {
        Settings::CpuAccuracy accuracy = Settings::CpuAccuracy::Auto;
        if (strcmp(value, "Auto") == 0) accuracy = Settings::CpuAccuracy::Auto;
        else if (strcmp(value, "Accurate") == 0) accuracy = Settings::CpuAccuracy::Accurate;
        else if (strcmp(value, "Unsafe") == 0) accuracy = Settings::CpuAccuracy::Unsafe;
        else if (strcmp(value, "Paranoid") == 0) accuracy = Settings::CpuAccuracy::Paranoid;
        Settings::values.cpu_accuracy.SetValue(accuracy);
        LOG_INFO(Frontend, "CPU Accuracy: {}", value);
    }

    // Async GPU (force off for libretro, but read in case user changes)
    if (const char* value = GetCoreOptionValue("eden_use_async_gpu")) {
        bool async_gpu = (strcmp(value, "enabled") == 0);
        // Always force to false for libretro
        Settings::values.use_asynchronous_gpu_emulation.SetValue(false);
        if (async_gpu) {
            LOG_WARNING(Frontend, "Async GPU requested but FORCED OFF for libretro compatibility");
        }
    }

    // Disk Shader Cache
    if (const char* value = GetCoreOptionValue("eden_disk_shader_cache")) {
        bool cache = (strcmp(value, "enabled") == 0);
        Settings::values.use_disk_shader_cache.SetValue(cache);
        LOG_INFO(Frontend, "Disk Shader Cache: {}", cache);
    }

    // Reactive Flushing
    if (const char* value = GetCoreOptionValue("eden_reactive_flushing")) {
        bool flushing = (strcmp(value, "enabled") == 0);
        Settings::values.use_reactive_flushing.SetValue(flushing);
        LOG_INFO(Frontend, "Reactive Flushing: {}", flushing);
    }

    // ASTC Recompression
    if (const char* value = GetCoreOptionValue("eden_astc_recompression")) {
        Settings::AstcRecompression recomp = Settings::AstcRecompression::Uncompressed;
        if (strcmp(value, "Uncompressed") == 0) recomp = Settings::AstcRecompression::Uncompressed;
        else if (strcmp(value, "Bc1") == 0) recomp = Settings::AstcRecompression::Bc1;
        else if (strcmp(value, "Bc3") == 0) recomp = Settings::AstcRecompression::Bc3;
        Settings::values.astc_recompression.SetValue(recomp);
        LOG_INFO(Frontend, "ASTC Recompression: {}", value);
    }

    // Performance optimizations - always enabled for libretro
    // Disable speed limit - let RetroArch handle frame pacing
    Settings::values.use_speed_limit.SetValue(false);

    // CRITICAL: Disable async shaders - worker threads crash without GL context
    Settings::values.use_asynchronous_shaders.SetValue(false);

    // Enable fastmem for better CPU performance
    Settings::values.cpuopt_fastmem.SetValue(true);
    Settings::values.cpuopt_fastmem_exclusives.SetValue(true);

    // Enable unsafe CPU optimizations for performance (can be toggled by CPU Accuracy setting)
    Settings::values.cpuopt_unsafe_unfuse_fma.SetValue(true);
    Settings::values.cpuopt_unsafe_reduce_fp_error.SetValue(true);
    Settings::values.cpuopt_unsafe_inaccurate_nan.SetValue(true);
    Settings::values.cpuopt_unsafe_fastmem_check.SetValue(true);
    Settings::values.cpuopt_unsafe_ignore_global_monitor.SetValue(true);

    // VSync off - RetroArch handles sync
    Settings::values.vsync_mode.SetValue(Settings::VSyncMode::Immediate);

    LOG_INFO(Frontend, "Libretro: Core options applied with performance optimizations");
}

// Context reset callback - called when OpenGL context is ready
void ContextReset() {
    try {
    LOG_INFO(Frontend, "Libretro: OpenGL context reset");
    hw_context_ready = true;

    if (emu_window) {
        emu_window->SetContextReady(true);
    }

    LibretroLog(RETRO_LOG_INFO, "Eden: OpenGL context ready\n");

    // Initialize OpenGL function pointers via GLAD
    if (hw_render.get_proc_address) {
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(hw_render.get_proc_address))) {
            LOG_ERROR(Frontend, "Libretro: Failed to load OpenGL functions via GLAD");
            LibretroLog(RETRO_LOG_ERROR, "Eden: Failed to initialize OpenGL\n");
            return;
        }
        LOG_INFO(Frontend, "Libretro: OpenGL functions loaded via GLAD - GL {}.{}",
                 GLVersion.major, GLVersion.minor);
        LibretroLog(RETRO_LOG_INFO, "Eden: OpenGL %d.%d initialized\n", GLVersion.major, GLVersion.minor);
    }

    // Load game now that OpenGL context is ready
    if (game_loaded && emu_system && !is_running && !game_path.empty()) {
        LOG_INFO(Frontend, "Libretro: Loading game with OpenGL context ready");

        Service::AM::FrontendAppletParameters load_params{
            .applet_id = Service::AM::AppletId::Application,
        };

        const Core::SystemResultStatus load_result = emu_system->Load(*emu_window, game_path, load_params);

        if (load_result != Core::SystemResultStatus::Success) {
            LOG_ERROR(Frontend, "Libretro: Failed to load game in context reset, error: {}", static_cast<u32>(load_result));
            LibretroLog(RETRO_LOG_ERROR, "Eden: Failed to load game, error: %u\n", static_cast<u32>(load_result));
            return;
        }

        // Start GPU and emulation
        LOG_INFO(Frontend, "Libretro: Starting GPU after successful load");
        emu_system->GPU().Start();
        emu_system->GetCpuManager().OnGpuReady();

        // Note: Shader cache preloading disabled for libretro - it spawns worker threads
        // that call MakeCurrent which crashes since only main thread has GL context.
        // Shaders will still be cached to disk and loaded on-demand.

        emu_system->Run();
        is_running = true;
        LibretroLog(RETRO_LOG_INFO, "Eden: Emulation started\n");
    }
    } catch (const std::exception& e) {
        LOG_CRITICAL(Frontend, "EXCEPTION in ContextReset: {}", e.what());
        LibretroLog(RETRO_LOG_ERROR, "Eden: CRITICAL - Exception in ContextReset: %s\n", e.what());
    } catch (...) {
        LOG_CRITICAL(Frontend, "UNKNOWN EXCEPTION in ContextReset");
        LibretroLog(RETRO_LOG_ERROR, "Eden: CRITICAL - Unknown exception in ContextReset\n");
    }
}

// Context destroy callback
void ContextDestroy() {
    try {
    LOG_INFO(Frontend, "Libretro: OpenGL context destroyed");
    hw_context_ready = false;

    if (emu_window) {
        emu_window->SetContextReady(false);
    }

    LibretroLog(RETRO_LOG_INFO, "Eden: OpenGL context destroyed\n");
    } catch (...) {
        // Ignore exceptions during cleanup
    }
}

// Get current framebuffer
uintptr_t GetCurrentFramebuffer() {
    if (emu_window) {
        return emu_window->GetCurrentFramebuffer();
    }
    return 0;
}

// Initialize hardware rendering
bool InitHWRender() {
    hw_render = {};
    hw_render.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
    hw_render.version_major = 4;
    hw_render.version_minor = 6;
    hw_render.context_reset = ContextReset;
    hw_render.context_destroy = ContextDestroy;
    hw_render.get_current_framebuffer = GetCurrentFramebuffer;
    hw_render.depth = true;
    hw_render.stencil = true;
    hw_render.bottom_left_origin = true;
    hw_render.cache_context = true;
    hw_render.debug_context = false;

    if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
        LOG_ERROR(Frontend, "Libretro: Failed to set HW render callback for OpenGL 4.6 Core");

        // Try OpenGL 4.3 Core
        hw_render.version_major = 4;
        hw_render.version_minor = 3;
        if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
            LOG_ERROR(Frontend, "Libretro: Failed to set HW render callback for OpenGL 4.3 Core");

            // Try OpenGL 3.3 Core
            hw_render.version_major = 3;
            hw_render.version_minor = 3;
            if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render)) {
                LOG_ERROR(Frontend, "Libretro: Failed to set HW render - no suitable OpenGL version");
                return false;
            }
        }
    }

    LOG_INFO(Frontend, "Libretro: HW render initialized with OpenGL {}.{} Core",
             hw_render.version_major, hw_render.version_minor);
    return true;
}

// Update input state using VirtualGamepad
void UpdateInput() {
    if (!input_subsystem || !input_poll_cb || !input_state_cb) {
        return;
    }

    try {

    input_poll_cb();

    auto* virtual_gamepad = input_subsystem->GetVirtualGamepad();
    if (!virtual_gamepad) return;

    using VB = InputCommon::VirtualGamepad::VirtualButton;
    using VS = InputCommon::VirtualGamepad::VirtualStick;

    // Map libretro buttons to VirtualGamepad buttons
    // Face buttons
    virtual_gamepad->SetButtonState(0, VB::ButtonA,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A) != 0);
    virtual_gamepad->SetButtonState(0, VB::ButtonB,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B) != 0);
    virtual_gamepad->SetButtonState(0, VB::ButtonX,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X) != 0);
    virtual_gamepad->SetButtonState(0, VB::ButtonY,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y) != 0);

    // D-Pad
    virtual_gamepad->SetButtonState(0, VB::ButtonUp,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) != 0);
    virtual_gamepad->SetButtonState(0, VB::ButtonDown,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN) != 0);
    virtual_gamepad->SetButtonState(0, VB::ButtonLeft,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT) != 0);
    virtual_gamepad->SetButtonState(0, VB::ButtonRight,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT) != 0);

    // Shoulder buttons
    virtual_gamepad->SetButtonState(0, VB::TriggerL,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L) != 0);
    virtual_gamepad->SetButtonState(0, VB::TriggerR,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R) != 0);
    virtual_gamepad->SetButtonState(0, VB::TriggerZL,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2) != 0);
    virtual_gamepad->SetButtonState(0, VB::TriggerZR,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2) != 0);

    // Stick buttons
    virtual_gamepad->SetButtonState(0, VB::StickL,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3) != 0);
    virtual_gamepad->SetButtonState(0, VB::StickR,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3) != 0);

    // Start/Select (Plus/Minus)
    virtual_gamepad->SetButtonState(0, VB::ButtonPlus,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START) != 0);
    virtual_gamepad->SetButtonState(0, VB::ButtonMinus,
        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT) != 0);

    // Analog sticks (normalize from -32768..32767 to -1.0..1.0)
    auto normalize_axis = [](int16_t value) -> float {
        return static_cast<float>(value) / 32767.0f;
    };

    float left_x = normalize_axis(input_state_cb(0, RETRO_DEVICE_ANALOG,
                                                  RETRO_DEVICE_INDEX_ANALOG_LEFT,
                                                  RETRO_DEVICE_ID_ANALOG_X));
    float left_y = normalize_axis(-input_state_cb(0, RETRO_DEVICE_ANALOG,
                                                   RETRO_DEVICE_INDEX_ANALOG_LEFT,
                                                   RETRO_DEVICE_ID_ANALOG_Y));
    virtual_gamepad->SetStickPosition(0, VS::Left, left_x, left_y);

    float right_x = normalize_axis(input_state_cb(0, RETRO_DEVICE_ANALOG,
                                                   RETRO_DEVICE_INDEX_ANALOG_RIGHT,
                                                   RETRO_DEVICE_ID_ANALOG_X));
    float right_y = normalize_axis(-input_state_cb(0, RETRO_DEVICE_ANALOG,
                                                    RETRO_DEVICE_INDEX_ANALOG_RIGHT,
                                                    RETRO_DEVICE_ID_ANALOG_Y));
    virtual_gamepad->SetStickPosition(0, VS::Right, right_x, right_y);
    } catch (const std::exception& e) {
        LOG_ERROR(Frontend, "Exception in UpdateInput: {}", e.what());
    }
}

// Render audio
void RenderAudio() {
    if (!emu_system || !audio_batch_cb) return;

    // Get audio samples from the audio core
    // TODO: Integrate with emu_system->AudioCore() for actual audio output
    const size_t samples_to_render = 800; // ~60fps at 48000Hz

    if (audio_buffer.size() < samples_to_render * 2) {
        audio_buffer.resize(samples_to_render * 2);
    }

    // Fill with silence for now - actual audio integration requires more work
    std::fill(audio_buffer.begin(), audio_buffer.end(), 0);

    audio_batch_cb(audio_buffer.data(), samples_to_render);
}

} // anonymous namespace

// Libretro API implementation
extern "C" {

void retro_set_environment(retro_environment_t cb) {
    environ_cb = cb;

    // Get log interface
    struct retro_log_callback log_callback;
    if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log_callback)) {
        log_cb = log_callback.log;
    }

    // We need fullpath for ROM loading
    bool need_fullpath = true;
    environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &need_fullpath);

    // Try v2 core options with categories first
    static struct retro_core_option_v2_category option_cats[] = {
        { "graphics", "Graphics", "Configure graphics rendering settings." },
        { "system", "System", "Configure system and emulation settings." },
        { "cpu", "CPU", "Configure CPU emulation settings." },
        { "advanced", "Advanced", "Advanced performance and optimization settings." },
        { nullptr, nullptr, nullptr }
    };

    static struct retro_core_option_v2_definition option_defs[] = {
        // Graphics Category
        {
            "eden_resolution_scale",
            "Internal Resolution",
            "Internal Resolution",
            "Rendering resolution multiplier. Higher values improve visual quality but reduce performance.",
            "Rendering resolution multiplier.",
            "graphics",
            {
                { "1x", "1x (1280x720)" },
                { "2x", "2x (2560x1440)" },
                { "3x", "3x (3840x2160)" },
                { "4x", "4x (5120x2880)" },
                { "5x", "5x (6400x3600)" },
                { "6x", "6x (7680x4320)" },
                { nullptr, nullptr }
            },
            "1x"
        },
        {
            "eden_gpu_accuracy",
            "GPU Accuracy",
            "GPU Accuracy",
            "Graphics emulation accuracy. Higher accuracy improves compatibility but reduces performance.",
            "Graphics emulation accuracy.",
            "graphics",
            {
                { "Low", "Low" },
                { "Medium", "Medium" },
                { "High", "High" },
                { nullptr, nullptr }
            },
            "Medium"
        },
        {
            "eden_scaling_filter",
            "Scaling Filter",
            "Scaling Filter",
            "Upscaling filter applied when internal resolution is higher than native. FSR provides good quality.",
            "Upscaling filter method.",
            "graphics",
            {
                { "Bilinear", "Bilinear" },
                { "Bicubic", "Bicubic" },
                { "Gaussian", "Gaussian" },
                { "ScaleForce", "ScaleForce" },
                { "Fsr", "FSR (AMD FidelityFX)" },
                { "NearestNeighbor", "Nearest Neighbor" },
                { nullptr, nullptr }
            },
            "Bilinear"
        },
        {
            "eden_anti_aliasing",
            "Anti-Aliasing",
            "Anti-Aliasing",
            "Post-process anti-aliasing. FXAA is faster, SMAA has better quality.",
            "Post-process anti-aliasing.",
            "graphics",
            {
                { "None", "None" },
                { "Fxaa", "FXAA" },
                { "Smaa", "SMAA" },
                { nullptr, nullptr }
            },
            "None"
        },
        {
            "eden_anisotropic_filtering",
            "Anisotropic Filtering",
            "Anisotropic Filtering",
            "Texture filtering quality. Higher values improve texture sharpness at angles.",
            "Texture filtering quality.",
            "graphics",
            {
                { "Automatic", "Automatic" },
                { "Default", "Default" },
                { "X2", "2x" },
                { "X4", "4x" },
                { "X8", "8x" },
                { "X16", "16x" },
                { nullptr, nullptr }
            },
            "Automatic"
        },
        {
            "eden_astc_decode",
            "ASTC Texture Decode",
            "ASTC Texture Decode",
            "Method for decoding ASTC textures. GPU is fastest. CPU Async balances quality and performance.",
            "ASTC texture decoding method.",
            "graphics",
            {
                { "Gpu", "GPU" },
                { "Cpu", "CPU" },
                { "CpuAsynchronous", "CPU Asynchronous" },
                { nullptr, nullptr }
            },
            "Gpu"
        },
        {
            "eden_shader_backend",
            "Shader Backend",
            "Shader Backend",
            "Shader compilation backend. SPIRV is recommended for better compatibility.",
            "Shader compilation backend.",
            "graphics",
            {
                { "SpirV", "SPIRV" },
                { "Glsl", "GLSL" },
                { "Glasm", "GLASM" },
                { nullptr, nullptr }
            },
            "SpirV"
        },
        // System Category
        {
            "eden_docked_mode",
            "Console Mode",
            "Console Mode",
            "Docked mode runs at higher resolution/performance. Handheld mode saves power.",
            "Switch console operating mode.",
            "system",
            {
                { "Docked", "Docked" },
                { "Handheld", "Handheld" },
                { nullptr, nullptr }
            },
            "Docked"
        },
        {
            "eden_language",
            "System Language",
            "System Language",
            "In-game language. Some games may not support all languages.",
            "System language setting.",
            "system",
            {
                { "EnglishAmerican", "English (US)" },
                { "EnglishBritish", "English (UK)" },
                { "Japanese", "Japanese" },
                { "French", "French" },
                { "German", "German" },
                { "Italian", "Italian" },
                { "Spanish", "Spanish" },
                { "Chinese", "Chinese" },
                { "Korean", "Korean" },
                { "Dutch", "Dutch" },
                { "Portuguese", "Portuguese" },
                { "Russian", "Russian" },
                { nullptr, nullptr }
            },
            "EnglishAmerican"
        },
        {
            "eden_region",
            "System Region",
            "System Region",
            "System region setting. Affects regional game content.",
            "System region.",
            "system",
            {
                { "Usa", "USA" },
                { "Europe", "Europe" },
                { "Japan", "Japan" },
                { "Australia", "Australia" },
                { "China", "China" },
                { "Korea", "Korea" },
                { "Taiwan", "Taiwan" },
                { nullptr, nullptr }
            },
            "Usa"
        },
        // CPU Category
        {
            "eden_use_multicore",
            "Multicore CPU Emulation",
            "Multicore CPU Emulation",
            "Enable multi-threaded CPU emulation. Improves performance on multi-core systems.",
            "Multi-threaded CPU emulation.",
            "cpu",
            {
                { "enabled", "Enabled" },
                { "disabled", "Disabled" },
                { nullptr, nullptr }
            },
            "enabled"
        },
        {
            "eden_cpu_accuracy",
            "CPU Accuracy",
            "CPU Accuracy",
            "CPU emulation accuracy. Auto is recommended. Unsafe improves performance but may cause issues.",
            "CPU emulation accuracy level.",
            "cpu",
            {
                { "Auto", "Auto" },
                { "Accurate", "Accurate" },
                { "Unsafe", "Unsafe" },
                { "Paranoid", "Paranoid" },
                { nullptr, nullptr }
            },
            "Auto"
        },
        // Advanced Category
        {
            "eden_use_async_gpu",
            "Async GPU Emulation",
            "Async GPU Emulation",
            "MUST be disabled for libretro. Separate GPU thread incompatible with RetroArch.",
            "Async GPU (libretro incompatible).",
            "advanced",
            {
                { "disabled", "Disabled (Required)" },
                { "enabled", "Enabled (Broken)" },
                { nullptr, nullptr }
            },
            "disabled"
        },
        {
            "eden_disk_shader_cache",
            "Disk Shader Cache",
            "Disk Shader Cache",
            "Cache compiled shaders to disk. Reduces stuttering on subsequent runs.",
            "Disk shader caching.",
            "advanced",
            {
                { "enabled", "Enabled" },
                { "disabled", "Disabled" },
                { nullptr, nullptr }
            },
            "enabled"
        },
        {
            "eden_reactive_flushing",
            "Reactive Memory Flushing",
            "Reactive Memory Flushing",
            "Memory synchronization optimization. Improves performance.",
            "Reactive memory flushing.",
            "advanced",
            {
                { "enabled", "Enabled" },
                { "disabled", "Disabled" },
                { nullptr, nullptr }
            },
            "enabled"
        },
        {
            "eden_astc_recompression",
            "ASTC Recompression",
            "ASTC Recompression",
            "Recompress ASTC textures to save VRAM. BC1 for opaque, BC3 for transparent.",
            "ASTC texture recompression.",
            "advanced",
            {
                { "Uncompressed", "Uncompressed" },
                { "Bc1", "BC1" },
                { "Bc3", "BC3" },
                { nullptr, nullptr }
            },
            "Uncompressed"
        },
        { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, {{ nullptr, nullptr }}, nullptr }
    };

    struct retro_core_options_v2 options_v2 = {
        option_cats,
        option_defs
    };

    // Try v2 first, fall back to v1 if not supported
    unsigned version = 0;
    if (environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && version >= 2) {
        environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &options_v2);
        LibretroLog(RETRO_LOG_INFO, "Eden: Using v2 core options\n");
    } else {
        // v1 fallback for older RetroArch
        static const struct retro_variable variables[] = {
            { "eden_resolution_scale", "Internal Resolution; 1x|2x|3x|4x|5x|6x" },
            { "eden_gpu_accuracy", "GPU Accuracy; Medium|Low|High" },
            { "eden_scaling_filter", "Scaling Filter; Bilinear|Bicubic|Gaussian|ScaleForce|Fsr|NearestNeighbor" },
            { "eden_anti_aliasing", "Anti-Aliasing; None|Fxaa|Smaa" },
            { "eden_anisotropic_filtering", "Anisotropic Filtering; Automatic|Default|X2|X4|X8|X16" },
            { "eden_astc_decode", "ASTC Texture Decode; Gpu|Cpu|CpuAsynchronous" },
            { "eden_shader_backend", "Shader Backend; SpirV|Glsl|Glasm" },
            { "eden_docked_mode", "Console Mode; Docked|Handheld" },
            { "eden_language", "System Language; EnglishAmerican|EnglishBritish|Japanese|French|German|Italian|Spanish|Chinese|Korean|Dutch|Portuguese|Russian" },
            { "eden_region", "System Region; Usa|Europe|Japan|Australia|China|Korea|Taiwan" },
            { "eden_use_multicore", "Multicore CPU Emulation; enabled|disabled" },
            { "eden_cpu_accuracy", "CPU Accuracy; Auto|Accurate|Unsafe|Paranoid" },
            { "eden_use_async_gpu", "Async GPU Emulation; disabled|enabled" },
            { "eden_disk_shader_cache", "Disk Shader Cache; enabled|disabled" },
            { "eden_reactive_flushing", "Reactive Memory Flushing; enabled|disabled" },
            { "eden_astc_recompression", "ASTC Recompression; Uncompressed|Bc1|Bc3" },
            { nullptr, nullptr }
        };
        environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)variables);
        LibretroLog(RETRO_LOG_INFO, "Eden: Using v1 core options\n");
    }

    // Set input descriptors
    static const struct retro_input_descriptor input_desc[] = {
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "A" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "X" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "Y" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "L" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "R" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "ZL" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "ZR" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3,    "Left Stick" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3,    "Right Stick" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Plus" },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT,"Minus" },
        { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Left Analog X" },
        { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Left Analog Y" },
        { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X, "Right Analog X" },
        { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y, "Right Analog Y" },
        { 0, 0, 0, 0, nullptr }
    };
    environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void*)input_desc);

    LibretroLog(RETRO_LOG_INFO, "Eden: Environment set\n");
}

void retro_set_video_refresh(retro_video_refresh_t cb) {
    video_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb) {
    audio_sample_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {
    audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb) {
    input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb) {
    input_state_cb = cb;
}

void retro_init(void) {
    LOG_INFO(Frontend, "Libretro: retro_init called");

    // Set main thread ID for OpenGL context tracking
    Libretro::LibretroGraphicsContext::SetMainThreadId();

    // Initialize logging
    Common::Log::Initialize();
    Common::Log::SetColorConsoleBackendEnabled(true);
    Common::Log::Start();

    // Initialize detached tasks
    detached_tasks = std::make_unique<Common::DetachedTasks>();

    // Initialize input subsystem
    input_subsystem = std::make_unique<InputCommon::InputSubsystem>();
    input_subsystem->Initialize();

    // Get system directory
    const char* dir = nullptr;
    if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir) {
        system_directory = dir;
        // Tell Eden's path manager to use RetroArch's system directory
        Common::FS::SetAppDirectory(system_directory);
    } else {
        system_directory = ".";
    }

    // Get save directory
    if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir) && dir) {
        save_directory = dir;
    } else {
        save_directory = ".";
    }

    // Set pixel format
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);

    // Request shared context for multi-threaded GL
    bool shared_context = true;
    environ_cb(RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT, &shared_context);

    is_initialized = true;

    LibretroLog(RETRO_LOG_INFO, "Eden: Initialized - System dir: %s, Save dir: %s\n",
                system_directory.c_str(), save_directory.c_str());
}

void retro_deinit(void) {
    LOG_INFO(Frontend, "Libretro: retro_deinit called");
    LibretroLog(RETRO_LOG_INFO, "Eden: Deinitializing (frame_count: %llu)\n", frame_count.load());

    // Mark as not running first to stop any processing
    is_running = false;
    is_initialized = false;
    hw_context_ready = false;

    // Shutdown system if still active (may have been shut down by retro_unload_game)
    if (emu_system) {
        try {
            // Notify GPU of shutdown first
            if (emu_system->IsPoweredOn()) {
                emu_system->GPU().NotifyShutdown();
            }
            emu_system->ShutdownMainProcess();
        } catch (...) {
            // Ignore errors during shutdown
        }
        emu_system.reset();
    }

    // Reset window after system
    emu_window.reset();

    // Shutdown input
    if (input_subsystem) {
        try {
            input_subsystem->Shutdown();
        } catch (...) {}
        input_subsystem.reset();
    }

    // Wait for detached tasks
    if (detached_tasks) {
        try {
            detached_tasks->WaitForAllTasks();
        } catch (...) {}
        detached_tasks.reset();
    }

    LibretroLog(RETRO_LOG_INFO, "Eden: Deinitialized\n");
}

unsigned retro_api_version(void) {
    return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info* info) {
    std::memset(info, 0, sizeof(*info));
    info->library_name = "Eden";
    info->library_version = Common::g_scm_desc;
    info->valid_extensions = "nsp|xci|nca|nso|nro";
    info->need_fullpath = true;
    info->block_extract = true;
}

void retro_get_system_av_info(struct retro_system_av_info* info) {
    std::memset(info, 0, sizeof(*info));

    info->geometry.base_width = SCREEN_WIDTH;
    info->geometry.base_height = SCREEN_HEIGHT;
    info->geometry.max_width = SCREEN_WIDTH * 4;
    info->geometry.max_height = SCREEN_HEIGHT * 4;
    info->geometry.aspect_ratio = static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT);

    info->timing.fps = FPS;
    info->timing.sample_rate = SAMPLE_RATE;
}

void retro_set_controller_port_device(unsigned port, unsigned device) {
    LOG_INFO(Frontend, "Libretro: Set controller port {} to device {}", port, device);
}

void retro_reset(void) {
    LOG_INFO(Frontend, "Libretro: retro_reset called");

    // Reset the emulation
    if (emu_system && is_running) {
        // Pause, reset state, and resume
        emu_system->Pause();
        // Full reset would require reloading the game
        emu_system->Run();
    }
}

void retro_run(void) {
    frame_count++;

    if (!emu_system || !is_running) {
        if (frame_count % 300 == 0) {
            LOG_WARNING(Frontend, "retro_run called but emulation not running (frame {})", frame_count.load());
        }
        if (video_cb) {
            video_cb(RETRO_HW_FRAME_BUFFER_VALID, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
        }
        return;
    }

    try {
        // Update input - wrap in try/catch for safety
        if (input_subsystem) {
            UpdateInput();
        }

        // Present the frame via HW rendering
        if (hw_context_ready && video_cb) {
            uintptr_t ra_fbo = hw_render.get_current_framebuffer ? hw_render.get_current_framebuffer() : 0;
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(ra_fbo));
            video_cb(RETRO_HW_FRAME_BUFFER_VALID, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
        }

        // Render audio
        if (audio_batch_cb) {
            RenderAudio();
        }

    } catch (const std::exception& e) {
        LOG_ERROR(Frontend, "Exception in retro_run: {}", e.what());
        is_running = false;
    } catch (...) {
        LOG_ERROR(Frontend, "Unknown exception in retro_run");
        is_running = false;
    }
}

size_t retro_serialize_size(void) {
    // Save states not yet supported
    return 0;
}

bool retro_serialize(void* data, size_t size) {
    (void)data;
    (void)size;
    return false;
}

bool retro_unserialize(const void* data, size_t size) {
    (void)data;
    (void)size;
    return false;
}

void retro_cheat_reset(void) {
    // Cheats not yet supported
}

void retro_cheat_set(unsigned index, bool enabled, const char* code) {
    (void)index;
    (void)enabled;
    (void)code;
}

bool retro_load_game(const struct retro_game_info* game) {
    if (!game || !game->path) {
        LOG_ERROR(Frontend, "Libretro: No game provided");
        return false;
    }

    LOG_INFO(Frontend, "Libretro: Loading game: {}", game->path);
    LibretroLog(RETRO_LOG_INFO, "Eden: Loading game: %s\n", game->path);

    game_path = game->path;

    // Initialize hardware rendering
    if (!InitHWRender()) {
        LOG_ERROR(Frontend, "Libretro: Failed to initialize hardware rendering");
        return false;
    }

    // Create emulator window
    emu_window = std::make_unique<Libretro::EmuWindowLibretro>();
    emu_window->SetHWRenderCallback(&hw_render);
    emu_window->SetFramebufferSize(SCREEN_WIDTH, SCREEN_HEIGHT);

    // Create and initialize the system
    emu_system = std::make_unique<Core::System>();
    emu_system->Initialize();

    // Configure settings for libretro
    Settings::values.renderer_backend = Settings::RendererBackend::OpenGL_GLSL;
    Settings::values.use_speed_limit.SetValue(false);
    Settings::values.use_multi_core.SetValue(true);
    Settings::values.use_disk_shader_cache.SetValue(true);
    // CRITICAL: Use sync GPU mode for libretro - OpenGL context is only valid on main thread
    Settings::values.use_asynchronous_gpu_emulation.SetValue(false);

    // Read core options from RetroArch
    ApplyCoreOptions();

    // Set up filesystem
    emu_system->SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
    emu_system->SetFilesystem(std::make_shared<FileSys::RealVfsFilesystem>());
    emu_system->GetFileSystemController().CreateFactories(*emu_system->GetFilesystem());
    emu_system->GetUserChannel().clear();

    // Mark game as ready to load - actual Load() will happen in ContextReset when OpenGL is ready
    game_loaded = true;

    LOG_INFO(Frontend, "Libretro: Game setup complete, waiting for OpenGL context");
    LibretroLog(RETRO_LOG_INFO, "Eden: Ready to load game\n");

    return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info* info, size_t num_info) {
    (void)game_type;
    (void)info;
    (void)num_info;
    return false;
}

void retro_unload_game(void) {
    LOG_INFO(Frontend, "Libretro: Unloading game");

    // Stop running first
    is_running = false;
    game_loaded = false;
    hw_context_ready = false;

    if (emu_system) {
        try {
            // Pause first
            emu_system->Pause();

            // Notify GPU shutdown before main shutdown
            if (emu_system->IsPoweredOn()) {
                emu_system->GPU().NotifyShutdown();
                // Give GPU time to process shutdown
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            emu_system->ShutdownMainProcess();
        } catch (...) {
            // Ignore errors during shutdown
        }
        emu_system.reset();
    }

    // Don't reset emu_window here - let retro_deinit handle it
    // This avoids double-free issues

    game_path.clear();

    LibretroLog(RETRO_LOG_INFO, "Eden: Game unloaded\n");
}

unsigned retro_get_region(void) {
    return RETRO_REGION_NTSC;
}

void* retro_get_memory_data(unsigned id) {
    (void)id;
    return nullptr;
}

size_t retro_get_memory_size(unsigned id) {
    (void)id;
    return 0;
}

} // extern "C"

// VMA implementation - must be in exactly one translation unit
#define VMA_IMPLEMENTATION
#include "video_core/vulkan_common/vma.h"
