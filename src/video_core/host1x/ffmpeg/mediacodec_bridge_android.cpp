// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Android-specific JNI bridge implementation
#ifdef __ANDROID__

#include "mediacodec_bridge.h"
#include <jni.h>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <cstdint>
#include "common/android/id_cache.h"
#include "common/logging/log.h"
#include <android/hardware_buffer_jni.h>
#include <deque>
#include <queue>

namespace FFmpeg::MediaCodecBridge {

static jclass g_native_media_codec_class = nullptr;
static jmethodID g_create_decoder = nullptr;
static jmethodID g_release_decoder = nullptr;
static jmethodID g_decode_method = nullptr;

struct DecoderState {
    int id;
    std::mutex mtx;
    std::vector<uint8_t> frame;
    int width = 0;
    int height = 0;
    int64_t pts = 0;
    bool has_frame = false;
    // Queue of AHardwareBuffer pointers for zero-copy path.
    std::deque<AHardwareBuffer*> hw_queue;
    std::deque<int64_t> hw_pts_queue;
};

static std::mutex s_global_mtx;
static std::map<int, std::shared_ptr<DecoderState>> s_decoders;

#ifdef __ANDROID__
// Global queue for AHardwareBuffer presentation (producer: mediacodec, consumer: PresentManager)
static std::mutex s_present_queue_mtx;
static std::deque<AHardwareBuffer*> s_present_queue_ahb;
static std::deque<int> s_present_queue_w;
static std::deque<int> s_present_queue_h;
static std::deque<int64_t> s_present_queue_pts;

void EnqueueHardwareBufferForPresent(AHardwareBuffer* ahb, int width, int height, int64_t pts) {
    std::lock_guard lock(s_present_queue_mtx);
    s_present_queue_ahb.push_back(ahb);
    s_present_queue_w.push_back(width);
    s_present_queue_h.push_back(height);
    s_present_queue_pts.push_back(pts);
}

bool TryPopHardwareBufferForPresent(AHardwareBuffer** out_ahb, int& out_width, int& out_height, int64_t& out_pts) {
    std::lock_guard lock(s_present_queue_mtx);
    if (s_present_queue_ahb.empty()) return false;
    *out_ahb = s_present_queue_ahb.front();
    out_width = s_present_queue_w.front();
    out_height = s_present_queue_h.front();
    out_pts = s_present_queue_pts.front();
    s_present_queue_ahb.pop_front();
    s_present_queue_w.pop_front();
    s_present_queue_h.pop_front();
    s_present_queue_pts.pop_front();
    return true;
}
#endif

extern "C" JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_media_NativeMediaCodec_onFrameDecoded(
    JNIEnv* env, jclass, jint decoderId, jbyteArray data, jint width, jint height, jlong pts) {
    std::lock_guard lock(s_global_mtx);
    auto it = s_decoders.find(decoderId);
    if (it == s_decoders.end()) return;
    auto& st = it->second;
    const jsize len = env->GetArrayLength(data);
    st->frame.resize(len);
    env->GetByteArrayRegion(data, 0, len, reinterpret_cast<jbyte*>(st->frame.data()));
    st->width = width;
    st->height = height;
    st->pts = pts;
    st->has_frame = true;
}

extern "C" JNIEXPORT void JNICALL Java_org_yuzu_yuzu_1emu_media_NativeMediaCodec_onHardwareBufferAvailable(
    JNIEnv* env, jclass, jint decoderId, jobject hardwareBuffer, jlong pts) {
    if (hardwareBuffer == nullptr) return;
    std::lock_guard lock(s_global_mtx);
    auto it = s_decoders.find(decoderId);
    if (it == s_decoders.end()) return;
    auto& st = it->second;

    // Convert Java HardwareBuffer to AHardwareBuffer*
    AHardwareBuffer* ahb = AHardwareBuffer_fromHardwareBuffer(env, hardwareBuffer);
    if (!ahb) return;
    // Acquire to extend lifetime
    AHardwareBuffer_acquire(ahb);

    // Optionally get description to set width/height for caller convenience
    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(ahb, &desc);

    st->hw_queue.push_back(ahb);
    st->hw_pts_queue.push_back(static_cast<int64_t>(pts));
    LOG_DEBUG(Render_Vulkan, "Enqueued AHardwareBuffer for decoder {} size={}x{} pts={}", decoderId, desc.width, desc.height, (long long)pts);
    st->has_frame = true;
    // Also enqueue for presentation (global present queue). PresentManager will consume these.
    EnqueueHardwareBufferForPresent(ahb, static_cast<int>(desc.width), static_cast<int>(desc.height), static_cast<int64_t>(pts));
}

bool IsAvailable() {
    // We assume the bridge is available if the Java class can be found.
    auto env = Common::Android::GetEnvForThread();
    if (!env) return false;
    if (!g_native_media_codec_class) {
        jclass cls = env->FindClass("org/yuzu/yuzu_emu/media/NativeMediaCodec");
        if (!cls) return false;
        g_native_media_codec_class = reinterpret_cast<jclass>(env->NewGlobalRef(cls));
        g_create_decoder = env->GetStaticMethodID(g_native_media_codec_class, "createDecoder", "(Ljava/lang/String;II)I");
        g_release_decoder = env->GetStaticMethodID(g_native_media_codec_class, "releaseDecoder", "(I)V");
        g_decode_method = env->GetStaticMethodID(g_native_media_codec_class, "decode", "(I[BJ)Z");
    }
    return g_native_media_codec_class != nullptr;
}

int CreateDecoder(const char* mime, int width, int height) {
    auto env = Common::Android::GetEnvForThread();
    if (!env) return 0;
    jstring jmime = env->NewStringUTF(mime);
    const int id = env->CallStaticIntMethod(g_native_media_codec_class, g_create_decoder, jmime, width, height);
    env->DeleteLocalRef(jmime);
    if (id <= 0) return 0;
    std::lock_guard lock(s_global_mtx);
    auto st = std::make_shared<DecoderState>();
    st->id = id;
    s_decoders[id] = st;
    return id;
}

void DestroyDecoder(int id) {
    auto env = Common::Android::GetEnvForThread();
    if (!env) return;
    env->CallStaticVoidMethod(g_native_media_codec_class, g_release_decoder, id);
    std::lock_guard lock(s_global_mtx);
    s_decoders.erase(id);
}

bool SendPacket(int id, const uint8_t* data, size_t size, int64_t pts) {
    auto env = Common::Android::GetEnvForThread();
    if (!env) return false;
    std::lock_guard lock(s_global_mtx);
    auto it = s_decoders.find(id);
    if (it == s_decoders.end()) return false;
    jbyteArray arr = env->NewByteArray(static_cast<jsize>(size));
    env->SetByteArrayRegion(arr, 0, static_cast<jsize>(size), reinterpret_cast<const jbyte*>(data));
    jboolean ok = env->CallStaticBooleanMethod(g_native_media_codec_class, g_decode_method, id, arr, static_cast<jlong>(pts));
    env->DeleteLocalRef(arr);
    return ok;
}

std::optional<std::vector<uint8_t>> PopDecodedFrame(int id, int& width, int& height, int64_t& pts) {
    std::lock_guard lock(s_global_mtx);
    auto it = s_decoders.find(id);
    if (it == s_decoders.end()) return std::nullopt;
    auto& st = it->second;
    if (!st->has_frame) return std::nullopt;
    st->has_frame = false;
    width = st->width;
    height = st->height;
    pts = st->pts;
    return st->frame;
}

} // namespace FFmpeg::MediaCodecBridge

#ifdef __ANDROID__
std::optional<AHardwareBuffer*> FFmpeg::MediaCodecBridge::PopDecodedHardwareBuffer(int id, int& width, int& height, int64_t& pts) {
    std::lock_guard lock(s_global_mtx);
    auto it = s_decoders.find(id);
    if (it == s_decoders.end()) return std::nullopt;
    auto& st = it->second;
    if (st->hw_queue.empty()) return std::nullopt;
    AHardwareBuffer* ahb = st->hw_queue.front();
    st->hw_queue.pop_front();
    pts = st->hw_pts_queue.front();
    st->hw_pts_queue.pop_front();
    // Fill width/height from the buffer description
    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(ahb, &desc);
    width = static_cast<int>(desc.width);
    height = static_cast<int>(desc.height);
    return std::optional<AHardwareBuffer*>{ahb};
}
#endif

#endif // __ANDROID__
