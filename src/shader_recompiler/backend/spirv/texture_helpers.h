// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "shader_recompiler/profile.h"
#include "shader_recompiler/shader_info.h"

namespace Shader::Backend::SPIRV {

inline bool Is1DTexture(TextureType type) noexcept {
    return type == TextureType::Color1D || type == TextureType::ColorArray1D;
}

inline bool Needs1DPromotion(const Profile& profile, TextureType type) noexcept {
    return !profile.support_sampled_1d && Is1DTexture(type);
}

inline TextureType EffectiveTextureType(const Profile& profile, TextureType type) noexcept {
    if (!Needs1DPromotion(profile, type)) {
        return type;
    }
    if (type == TextureType::Color1D) {
        return TextureType::Color2D;
    }
    return TextureType::ColorArray2D;
}

} // namespace Shader::Backend::SPIRV
