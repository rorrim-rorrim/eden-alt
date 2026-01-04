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

#include "eden_libretro/libretro.h"
#include "eden_libretro/libretro_vfs.h"
#include "eden_libretro/emu_window_libretro.h"

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
std::atomic<bool> has_new_frame{false};
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
        
        // Enable deferred GPU mode - commands processed on main thread during retro_run
        LOG_INFO(Frontend, "Libretro: Enabling deferred GPU mode");
        emu_system->GPU().SetDeferredMode(true);
        
        // Start GPU and emulation
        LOG_INFO(Frontend, "Libretro: Starting GPU after successful load");
        emu_system->GPU().Start();
        emu_system->GetCpuManager().OnGpuReady();
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
    
    // Set core options
    static const struct retro_variable variables[] = {
        { "eden_resolution_scale", "Resolution Scale; 1x|2x|3x|4x" },
        { "eden_use_vsync", "VSync; On|Off" },
        { "eden_use_async_gpu", "Async GPU; On|Off" },
        { "eden_use_multicore", "Multicore CPU; On|Off" },
        { "eden_shader_backend", "Shader Backend; GLSL|SPIRV" },
        { nullptr, nullptr }
    };
    environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)variables);
    
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
    try {
    LOG_INFO(Frontend, "Libretro: retro_deinit called");
    LibretroLog(RETRO_LOG_INFO, "Eden: Deinitializing (frame_count: %llu)\n", frame_count.load());
    
    is_initialized = false;
    is_running = false;
    
    if (emu_system) {
        LOG_INFO(Frontend, "Shutting down emulation system");
        emu_system->ShutdownMainProcess();
        emu_system.reset();
        LOG_INFO(Frontend, "System shutdown complete");
    }
    
    emu_window.reset();
    
    if (input_subsystem) {
        input_subsystem->Shutdown();
        input_subsystem.reset();
    }
    
    if (detached_tasks) {
        detached_tasks->WaitForAllTasks();
        detached_tasks.reset();
    }
    
    LibretroLog(RETRO_LOG_INFO, "Eden: Deinitialized\n");
    } catch (const std::exception& e) {
        LOG_CRITICAL(Frontend, "EXCEPTION in retro_deinit: {}", e.what());
        if (log_cb) {
            log_cb(RETRO_LOG_ERROR, "Eden: Exception in deinit: %s\n", e.what());
        }
    } catch (...) {
        LOG_CRITICAL(Frontend, "UNKNOWN EXCEPTION in retro_deinit");
        if (log_cb) {
            log_cb(RETRO_LOG_ERROR, "Eden: Unknown exception in deinit\n");
        }
    }
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
        
        // Process pending GPU commands and composites on main thread (deferred mode)
        // Poll multiple times to allow game threads to make progress
        if (emu_system && hw_context_ready) {
            try {
                auto& gpu = emu_system->GPU();
                
                // Process commands multiple times with short sleeps to allow game threads to queue more
                for (int i = 0; i < 10; i++) {
                    gpu.ProcessPendingCommands();
                    gpu.ProcessPendingComposites();
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
                
                has_new_frame = false;
            } catch (...) {
                // GPU might not be ready yet
            }
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
        
        if (frame_count % 600 == 0) {
            LOG_INFO(Frontend, "retro_run: frame {} rendered", frame_count.load());
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
    Settings::values.renderer_backend = Settings::RendererBackend::OpenGL;
    Settings::values.use_speed_limit.SetValue(false);
    Settings::values.use_multi_core.SetValue(true);
    Settings::values.use_disk_shader_cache.SetValue(true);
    // CRITICAL: Use sync GPU mode for libretro - OpenGL context is only valid on main thread
    Settings::values.use_asynchronous_gpu_emulation.SetValue(false);
    
    // Apply settings
    emu_system->ApplySettings();
    
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
    
    is_running = false;
    game_loaded = false;
    
    if (emu_system) {
        emu_system->Pause();
        emu_system->ShutdownMainProcess();
        emu_system.reset();
    }
    
    emu_window.reset();
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
