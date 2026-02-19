// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "common/uuid.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/ns/notify_service.h"
#include "core/hle/service/service.h"
#include "core/launch_timestamp_cache.h"
#include "frontend_common/play_time_manager.h"

namespace Service::NS {

INotifyService::INotifyService(Core::System& system_)
    : ServiceFramework{system_, "pdm:ntfy"}
{
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "NotifyAppletEvent"},
        {2, nullptr, "NotifyOperationModeChangeEvent"},
        {3, nullptr, "NotifyPowerStateChangeEvent"},
        {4, nullptr, "NotifyClearAllEvent"},
        {5, nullptr, "NotifyEventForDebug"},
        {6, nullptr, "SuspendUserAccountEventService"},
        {7, nullptr, "ResumeUserAccountEventService"},
        {8, nullptr, "Unknown8"},
        {9, nullptr, "Unknown9"},
        {20, nullptr, "Unknown20"},
        {100, nullptr, "Unknown100"},
        {101, nullptr, "Unknown101"},

    };
    // clang-format on
    RegisterHandlers(functions);
}

INotifyService::~INotifyService() = default;

} // namespace Service::NS
