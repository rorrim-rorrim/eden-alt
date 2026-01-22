// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "video_core/vulkan_common/vulkan.h"

#if defined(__APPLE__) && !defined(VK_STRUCTURE_TYPE_OH_SURFACE_CREATE_INFO_OHOS)
#   define VK_STRUCTURE_TYPE_OH_SURFACE_CREATE_INFO_OHOS VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS
#endif
#include <vulkan/vk_enum_string_helper.h>
