// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if defined(_WIN32)
#include <client/windows/handler/exception_handler.h>
#elif defined(__linux__)
#include <client/linux/handler/exception_handler.h>
#elif defined(__sun__)
#include <client/solaris/handler/exception_handler.h>
#else
#error Minidump creation not supported on this platform
#endif

namespace Breakpad {

void InstallCrashHandler();
#if defined(__linux__) || defined(__sun__)
[[noreturn]] bool DumpCallback(const google_breakpad::MinidumpDescriptor& descriptor,
                  void* context,
                  bool succeeded);
#endif

}
