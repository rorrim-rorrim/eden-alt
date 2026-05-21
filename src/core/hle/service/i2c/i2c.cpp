// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/service/i2c/i2c.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/service.h"

namespace Service::I2C {

class I2C final : public ServiceFramework<I2C> {
public:
    explicit I2C(Core::System& system_)
        : ServiceFramework{system_, "i2c"}
    {
        static const FunctionInfo functions[] = {
            {0, nullptr, "Cmd0"},
        };
        RegisterHandlers(functions);
    }
    ~I2C() override = default;
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);
    server_manager->RegisterNamedService("i2c", std::make_shared<I2C>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::I2C
