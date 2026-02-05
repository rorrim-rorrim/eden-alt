#pragma once

#include <QString>
#include "common/common_types.h"

namespace QtCommon::Mod {

QString GetModFolder(const QString &root, const QString &fallbackName);

bool InstallMod(const QString &path, const QString &fallbackName, const u64 program_id, const bool copy = true);

bool InstallModFromZip(const QString &path, const u64 program_id);

}
