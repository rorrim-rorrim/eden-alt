#pragma once

#include <filesystem>
#include <optional>
#include "common/common_types.h"

namespace FrontendCommon {

std::optional<std::filesystem::path> GetModFolder(const std::string& root);

bool InstallMod(const std::filesystem::path &path, const u64 program_id, const bool copy = true);
}
