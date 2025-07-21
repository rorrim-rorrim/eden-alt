// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef _WIN32
#include <wincon.h>
#include <windows.h>
#endif

#ifdef __APPLE__
#include <cstdio>
#include <string>
#include <vector>
#include <QProcess>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <unistd.h>

#include "common/fs/path_util.h"

extern char** environ;
#endif

#include "common/logging/backend.h"
#include "yuzu/debugger/console.h"
#include "yuzu/uisettings.h"

namespace Debugger {
#ifdef __APPLE__
namespace {

pid_t g_terminal_pid = -1;
int g_saved_out = -1;
int g_saved_err = -1;

std::string GetLogFilePath() {
    std::string dir = Common::FS::GetEdenPathString(Common::FS::EdenPath::LogDir);
    if (!dir.empty() && dir.back() != '/')
        dir.push_back('/');
    return dir + "eden_log.txt";
}

void ShowMacConsole() {
    const std::string logfile = GetLogFilePath();
    if (logfile.empty())
        return;

    g_saved_out = dup(STDOUT_FILENO);
    g_saved_err = dup(STDERR_FILENO);

    int fd = ::open(logfile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1)
        return;
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

const std::string shell_cmd =
    "osascript -e 'tell application \"Terminal\" "
    "to do script \"printf \\\\e]1;Eden logs\\\\a\\\\e]2;Eden logs\\\\a; "
    "tail -n +1 -F " + logfile + "\"'";

    posix_spawnp(&g_terminal_pid, "/bin/sh", nullptr, nullptr,
                 (char* const[]){const_cast<char*>("/bin/sh"), const_cast<char*>("-c"),
                                 const_cast<char*>(shell_cmd.c_str()), nullptr},
                 environ);
}

void HideMacConsole() {
    if (g_terminal_pid > 0) {
        kill(g_terminal_pid, SIGTERM);
        g_terminal_pid = -1;
    }
    if (g_saved_out != -1) {
        dup2(g_saved_out, STDOUT_FILENO);
        close(g_saved_out);
        g_saved_out = -1;
    }
    if (g_saved_err != -1) {
        dup2(g_saved_err, STDERR_FILENO);
        close(g_saved_err);
        g_saved_err = -1;
    }
}
} // namespace
#endif // __APPLE__

void ToggleConsole() {
    static bool console_shown = false;
    const bool want_console = UISettings::values.show_console.GetValue();
    if (console_shown == want_console)
        return;
    console_shown = want_console;

    using namespace Common::Log;
#if defined(_WIN32) && !defined(_DEBUG)
    FILE* temp;
    if (want_console) {
        if (AllocConsole()) {
            // The first parameter for freopen_s is a out parameter, so we can just ignore it
            freopen_s(&temp, "CONIN$", "r", stdin);
            freopen_s(&temp, "CONOUT$", "w", stdout);
            freopen_s(&temp, "CONOUT$", "w", stderr);
            SetConsoleOutputCP(65001);
            SetColorConsoleBackendEnabled(true);
        }
    } else {
        if (FreeConsole()) {
            // In order to close the console, we have to also detach the streams on it.
            // Just redirect them to NUL if there is no console window
            SetColorConsoleBackendEnabled(false);
            freopen_s(&temp, "NUL", "r", stdin);
            freopen_s(&temp, "NUL", "w", stdout);
            freopen_s(&temp, "NUL", "w", stderr);
        }
    }
#else
#if defined(__APPLE__)
    if (want_console)
        ShowMacConsole();
    else
        HideMacConsole();
#endif
    SetColorConsoleBackendEnabled(UISettings::values.show_console.GetValue());
#endif
}
} // namespace Debugger
