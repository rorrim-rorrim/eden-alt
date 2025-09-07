/* This file is part of the dynarmic project.
 * Copyright (c) 2023 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <mcl/macro/architecture.hpp>

#if defined(ARCHITECTURE_x86_64)
#    include "dynarmic/backend/x64/exception_handler_windows.cpp"
#elif defined(ARCHITECTURE_arm64)
#    include "dynarmic/backend/exception_handler_generic.cpp"
#else
#    error "Invalid architecture"
#endif
