// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <type_traits>
#include <fmt/ranges.h>

// Opt-out trait for enums that have their own custom fmt::formatter specialization.
namespace Common::Logging {
template <class T>
struct enable_generic_enum_formatter : std::true_type {};
} // namespace Common::Logging

#if FMT_VERSION >= 80100
template <typename T>
struct fmt::formatter<
    T,
    std::enable_if_t<
        std::is_enum_v<T> &&
        Common::Logging::enable_generic_enum_formatter<T>::value,
        char>>
    : fmt::formatter<std::underlying_type_t<T>> {
    template <typename FormatContext>
    auto format(const T& value, FormatContext& ctx) const -> decltype(ctx.out()) {
        return fmt::formatter<std::underlying_type_t<T>>::format(
            static_cast<std::underlying_type_t<T>>(value), ctx);
    }
};
#endif
