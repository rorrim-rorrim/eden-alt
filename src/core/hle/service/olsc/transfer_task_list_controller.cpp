// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/olsc/native_handle_holder.h"
#include "core/hle/service/olsc/transfer_task_list_controller.h"

namespace Service::OLSC {

ITransferTaskListController::ITransferTaskListController(Core::System& system_)
    : ServiceFramework{system_, "ITransferTaskListController"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "GetTransferTaskCountForOcean"},
        {1, nullptr, "GetTransferTaskInfoForOcean"},
        {2, nullptr, "ListTransferTaskInfoForOcean"},
        {3, nullptr, "DeleteTransferTaskForOcean"},
        {4, nullptr, "RaiseTransferTaskPriorityForOcean"},
        {5, D<&ITransferTaskListController::GetNativeHandleHolder>, "GetTransferTaskEndEventNativeHandleHolder"},
        {6, nullptr, "GetTransferTaskProgressForOcean"},
        {7, nullptr, "GetTransferTaskLastResultForOcean"},
        {8, nullptr, "StopNextTransferTaskExecution"},
        {9, D<&ITransferTaskListController::GetNativeHandleHolder>, "GetTransferTaskStartEventNativeHandleHolder"},
        {10, nullptr, "SuspendTransferTaskForOcean"},
        {11, nullptr, "GetCurrentTransferTaskInfoForOcean"},
        {12, nullptr, "FindTransferTaskInfoForOcean"},
        {13, nullptr, "CancelCurrentRepairTransferTask"}, // 9.0.0+
        {14, nullptr, "GetRepairTransferTaskProgress"}, // 9.0.0+
        {15, nullptr, "EnsureExecutableForRepairTransferTask"}, // 9.0.0+
        {16, nullptr, "GetTransferTaskCount"}, // 10.1.0+
        {17, nullptr, "GetTransferTaskInfo"}, // 10.1.0+
        {18, nullptr, "ListTransferTaskInfo"}, // 10.1.0+
        {19, nullptr, "DeleteTransferTask"}, // 10.1.0+
        {20, nullptr, "RaiseTransferTaskPriority"}, // 10.1.0+
        {21, nullptr, "GetTransferTaskProgress"}, // 10.1.0+
        {22, nullptr, "GetTransferTaskLastResult"}, // 10.1.0+
        {23, nullptr, "SuspendTransferTask"}, // 10.1.0+
        {24, nullptr, "GetCurrentTransferTaskInfo"}, // 10.1.0+
        {25, nullptr, "FindTransferTaskInfo"}, // 10.1.0+
        {26, nullptr, "Unknown26"}, // 20.1.0+
        {27, nullptr, "Unknown27"}, // 20.1.0+
        {28, nullptr, "Unknown28"}, // 20.1.0+
        {29, nullptr, "Unknown29"}, // 20.1.0+
        {30, nullptr, "Unknown30"}, // 20.1.0+
    };
    // clang-format on

    RegisterHandlers(functions);
}

ITransferTaskListController::~ITransferTaskListController() = default;

Result ITransferTaskListController::GetNativeHandleHolder(
    Out<SharedPointer<INativeHandleHolder>> out_holder) {
    LOG_WARNING(Service_OLSC, "(STUBBED) called");
    *out_holder = std::make_shared<INativeHandleHolder>(system);
    R_SUCCEED();
}

} // namespace Service::OLSC
