// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/olsc/remote_storage_controller.h"

namespace Service::OLSC {

IRemoteStorageController::IRemoteStorageController(Core::System& system_)
    : ServiceFramework{system_, "IRemoteStorageController"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "RegisterUploadSaveDataTransferTask"},
        {1, nullptr, "RegisterDownloadSaveDataTransferTask"},
        {2, nullptr, "Unknown2"}, // 6.0.0-7.0.1
        {3, nullptr, "GetCount"},
        {4, nullptr, "Unknown4"}, // 6.0.0-7.0.1
        {6, nullptr, "ClearDataInfoCache"},
        {7, nullptr, "RequestUpdateDataInfoCacheAsync"},
        {8, nullptr, "RequestUpdateDataInfoCacheOfSpecifiedApplicationAsync"},
        {9, nullptr, "DeleteDataInfoCache"},
        {10, nullptr, "GetDataNewness"},
        {11, nullptr, "RequestDeleteDataAsync"},
        {12, nullptr, "RegisterUploadSaveDataTransferTaskDetail"},
        {13, nullptr, "RequestRegisterNotificationTokenAsync"},
        {14, nullptr, "GetDataNewnessByApplicationId"},
        {15, nullptr, "RegisterUploadSaveDataTransferTaskForAutonomyRegistration"},
        {16, nullptr, "RequestCleanupToDeleteSaveDataArchiveAsync"},
        {17, nullptr, "ListDataInfo"}, // 7.0.0+
        {18, nullptr, "GetDataInfo"}, // 7.0.0+
        {19, nullptr, "GetDataInfoCacheUpdateNativeHandleHolder"}, // 7.0.0+
        {20, nullptr, "RequestUpdateSaveDataBackupInfoCacheAsync"}, // 10.1.0+
        {21, nullptr, "ListLoadedDataInfo"}, // 11.0.0+
        {22, D<&IRemoteStorageController::GetLoadedDataInfo>, "GetLoadedDataInfo"},
        {23, nullptr, "ApplyLoadedData"}, // 11.0.0+
        {24, nullptr, "DeleteLoadedData"}, // 11.0.0+
        {25, nullptr, "RegisterDownloadSaveDataTransferTaskForAutonomyRegistration"}, // 11.0.0+
        {26, nullptr, "Unknown26"}, // 20.0.0+
        {27, nullptr, "Unknown27"}, // 20.0.0+
        {800, nullptr, "Unknown800"}, // 20.0.0+
        {900, nullptr, "SetLoadedDataMissing"} // 11.0.0+
    };
    // clang-format on

    RegisterHandlers(functions);
}

IRemoteStorageController::~IRemoteStorageController() = default;

Result IRemoteStorageController::GetLoadedDataInfo(Out<bool> out_has_secondary_save,
                                                  Out<std::array<u64, 3>> out_unknown,
                                                  u64 application_id) {
    LOG_ERROR(Service_OLSC, "(STUBBED) called, application_id={:016X}", application_id);
    *out_has_secondary_save = false;
    *out_unknown = {};
    R_SUCCEED();
}

} // namespace Service::OLSC
