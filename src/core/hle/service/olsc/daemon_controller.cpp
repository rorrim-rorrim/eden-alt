// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/olsc/daemon_controller.h"

namespace Service::OLSC {

IDaemonController::IDaemonController(Core::System& system_)
    : ServiceFramework{system_, "IDaemonController"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, D<&IDaemonController::GetApplicationAutoTransferSetting>, "GetApplicationAutoTransferSetting"},
        {1, nullptr, "SetApplicationAutoTransferSetting"},
        {2, nullptr, "GetGlobalAutoUploadSetting"},
        {3, nullptr, "SetGlobalAutoUploadSetting"},
        {4, nullptr, "RunTransferTaskAutonomyRegistration"},
        {5, nullptr, "GetGlobalAutoDownloadSetting"}, // 11.0.0+
        {6, nullptr, "SetGlobalAutoDownloadSetting"}, // 11.0.0+
        {10, nullptr, "CreateForbiddenSaveDataInidication"},
        {11, nullptr, "StopAutonomyTaskExecution"},
        {12, nullptr, "GetAutonomyTaskStatus"},
        {13, nullptr, "Unknown13"}, // 20.0.0+
    };
    // clang-format on

    RegisterHandlers(functions);
}

IDaemonController::~IDaemonController() = default;

Result IDaemonController::GetApplicationAutoTransferSetting(Out<bool> out_is_enabled,
                                                                         Common::UUID user_id,
                                                                         u64 application_id) {
    LOG_WARNING(Service_OLSC, "(STUBBED) called, user_id={} application_id={:016X}",
                user_id.FormattedString(), application_id);
    *out_is_enabled = false;
    R_SUCCEED();
}

} // namespace Service::OLSC
