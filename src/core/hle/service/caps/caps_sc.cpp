// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/caps/caps_sc.h"

namespace Service::Capture {

IScreenShotControlService::IScreenShotControlService(Core::System& system_)
    : ServiceFramework{system_, "caps:sc"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {1, nullptr, "CaptureRawImageRgba32IntoArray"},
        {2, nullptr, "CaptureRawImageRgba32IntoArrayWithTimeout"},
        {3, nullptr, "AttachSharedBufferToCaptureModule"}, // 5.0.0+
        {5, nullptr, "CaptureRawImageToAttachedSharedBuffer"}, // 5.0.0+
        {210, nullptr, "SaveScreenShotEx2ViaAm"}, // 6.0.0+
        {1001, nullptr, "RequestTakingScreenShot"}, // 2.0.0-4.1.0
        {1002, nullptr, "RequestTakingScreenShotWithTimeout"}, // 2.0.0-4.1.0
        {1003, nullptr, "RequestTakingScreenShotEx"}, // 3.0.0-4.1.0
        {1004, nullptr, "RequestTakingScreenShotEx1"}, // 5.0.0+
        {1009, nullptr, "CancelTakingScreenShot"}, // 5.0.0+
        {1010, nullptr, "SetTakingScreenShotCancelState"}, // 5.0.0+
        {1011, nullptr, "NotifyTakingScreenShotRefused"},
        {1012, nullptr, "NotifyTakingScreenShotFailed"},
        {1100, nullptr, "Unknown1100"}, // 18.0.0+
        {1101, nullptr, "SetupOverlayMovieThumbnail"}, // 4.0.0+
        {1106, nullptr, "CreateProtoMovieMetaDataNv12Ex2"}, // 4.0.0+
        {1107, nullptr, "CreateProtoMovieMetaDataRgbaEx2"}, // 4.0.0+
        {1108, nullptr, "Unknown1108"}, // 18.0.0+
        {1109, nullptr, "Unknown1109"}, // 19.0.0+
        {1110, nullptr, "Unknown1110"}, // 19.0.0+
        {1111, nullptr, "Unknown1111"}, // 19.0.0+
        {1112, nullptr, "Unknown1112"}, // 19.0.0+
        {1113, nullptr, "Unknown1113"}, // 19.0.0+
        {1114, nullptr, "Unknown1114"}, // 19.0.0+
        {1201, nullptr, "OpenRawScreenShotReadStream"}, // 3.0.0+
        {1202, nullptr, "CloseRawScreenShotReadStream"}, // 3.0.0+
        {1203, nullptr, "ReadRawScreenShotReadStream"}, // 3.0.0+
        {1204, nullptr, "CaptureCrashScreenShot"}, // 9.0.0+
        {9000, nullptr, "Unknown9000"} // 20.0.0+
    };
    // clang-format on

    RegisterHandlers(functions);
}

IScreenShotControlService::~IScreenShotControlService() = default;

} // namespace Service::Capture
