// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "core/hle/service/mig/mig.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Service::Migration {

class MIG_USR final : public ServiceFramework<MIG_USR> {
public:
    explicit MIG_USR(Core::System& system_) : ServiceFramework{system_, "mig:usr"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown0"}, //19.0.0+
            {1, nullptr, "Unknown1"}, //20.0.0+
            {2, nullptr, "Unknown2"}, //20.0.0+
            {10, nullptr, "TryGetLastMigrationInfo"},
            {100, nullptr, "CreateUserMigrationServer"},
            {101, nullptr, "ResumeUserMigrationServer"},
            {200, nullptr, "CreateUserMigrationClient"},
            {201, nullptr, "ResumeUserMigrationClient"},
            {1001, nullptr, "GetSaveDataMigrationPolicyInfoAsync"},
            {1010, nullptr, "TryGetLastSaveDataMigrationInfo"},
            {1100, nullptr, "CreateSaveDataMigrationServer"},
            {1101, nullptr, "ResumeSaveDataMigrationServer"},
            {1200, nullptr, "CreateSaveDataMigrationClient"},
            {1201, nullptr, "ResumeSaveDataMigrationClient"}
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("mig:user", std::make_shared<MIG_USR>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::Migration
