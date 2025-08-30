/* SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project */
/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#define __ASSEMBLY__

#ifdef __APPLE__
/* https://cpip.readthedocs.io/en/stable/_static/dictobject.c/signal.h_bbe000f9714f274340a28e000a369354.html */
#define ReturnToRunCodeByExceptionLevelChangeSignal 31
#define BreakFromRunCodeSignal 16
#define GuestAccessFaultSignal 11
#define GuestAlignmentFaultSignal 10
#else
#include <asm-generic/signal.h>
#include <asm-generic/unistd.h>
#define ReturnToRunCodeByExceptionLevelChangeSignal SIGUSR2
#define BreakFromRunCodeSignal SIGURG
#define GuestAccessFaultSignal SIGSEGV
#define GuestAlignmentFaultSignal SIGBUS
#endif

#define GuestContextSp 0xF8
#define GuestContextHostContext 0x320

#define TpidrEl0NativeContext 0x10
#define TpidrEl0Lock 0x18
#define TpidrEl0TlsMagic 0x20
#define TlsMagic 0x555a5559

#define SpinLockLocked 0
#define SpinLockUnlocked 1

#define HostContextSpTpidrEl0 0xE0
#define HostContextTpidrEl0 0xE8
#define HostContextRegs 0x0
#define HostContextVregs 0x60
