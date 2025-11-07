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
};

static std::mutex s_global_mtx;
static std::map<int, std::shared_ptr<DecoderState>> s_decoders;

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

#endif // __ANDROID__
