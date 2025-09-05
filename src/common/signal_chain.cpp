// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <dlfcn.h>

#include "common/assert.h"
#include "common/dynamic_library.h"
#include "common/scope_exit.h"
#include "common/signal_chain.h"

namespace Common {

#ifdef __ANDROID__
template <typename T>
T* LookupLibcSymbol(const char* name) {
    Common::DynamicLibrary provider("libc.so");
    ASSERT_MSG(provider.IsOpen(), "Failed to open libc!");
    void* sym = provider.GetSymbolAddress(name);
    if (sym == nullptr) {
        sym = dlsym(RTLD_DEFAULT, name);
    }
    ASSERT_MSG(sym != nullptr, "Unable to find symbol {}!", name);
    return reinterpret_cast<T*>(sym);
}

int SigAction(int signum, const struct sigaction* act, struct sigaction* oldact) {
    static auto libc_sigaction = LookupLibcSymbol<decltype(sigaction)>("sigaction");
    return libc_sigaction(signum, act, oldact);
}
#else
int SigAction(int signum, const struct sigaction* act, struct sigaction* oldact) {
    return sigaction(signum, act, oldact);
}
#endif

} // namespace Common
