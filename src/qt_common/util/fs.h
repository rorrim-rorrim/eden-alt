// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/common_types.h"
#include <filesystem>

#pragma once

namespace QtCommon::FS {

void LinkRyujinx(std::filesystem::path &from, std::filesystem::path &to);
u64 GetRyujinxSaveID(const u64& program_id);

} // namespace QtCommon::FS
