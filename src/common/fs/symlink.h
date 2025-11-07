// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
namespace Common::FS {

bool CreateSymlink(std::filesystem::__cxx11::path from, std::filesystem::__cxx11::path to);
bool IsSymlink(const std::filesystem::path &path);

} // namespace Common::FS
