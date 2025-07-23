#include "qt_path_util.h"
#include <QDesktopServices>
#include <QString>
#include <QUrl>
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include <fmt/format.h>

bool QtCommon::PathUtil::OpenShaderCache(u64 program_id)
{
    const auto shader_cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ShaderDir);
    const auto shader_cache_folder_path{shader_cache_dir / fmt::format("{:016x}", program_id)};
    if (!Common::FS::CreateDirs(shader_cache_folder_path)) {
        return false;
    }

    const auto shader_path_string{Common::FS::PathToUTF8String(shader_cache_folder_path)};
    const auto qt_shader_cache_path = QString::fromStdString(shader_path_string);
    return QDesktopServices::openUrl(QUrl::fromLocalFile(qt_shader_cache_path));
}
