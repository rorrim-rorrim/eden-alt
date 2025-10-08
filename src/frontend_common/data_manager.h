#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "common/common_types.h"
#include <string>

namespace FrontendCommon::DataManager {

enum class DataDir { Saves, UserNand, SysNand, Mods, Shaders };

const std::string GetDataDir(DataDir dir);

u64 ClearDir(DataDir dir);

const std::string ReadableBytesSize(u64 size);

u64 DataDirSize(DataDir dir);

}; // namespace FrontendCommon::DataManager

#endif // DATA_MANAGER_H
