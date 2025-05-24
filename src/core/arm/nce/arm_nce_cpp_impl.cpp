#include "core/arm/nce/arm_nce_cpp_impl.h"

#include <cstring>
#include <sys/syscall.h>
#include <unistd.h>

namespace Core {

HaltReason ArmNce::ReturnToRunCodeByTrampoline(void* tpidr, GuestContext* ctx, uint64_t trampoline_addr) {
    // This function needs to be implemented with inline assembly because it 
    // involves saving/restoring registers and modifying the stack pointer
    HaltReason result;
    
    asm volatile(
        // Back up host sp and tpidr_el0
        "mov     x3, sp\n"
        "mrs     x4, tpidr_el0\n"
        
        // Load guest sp
        "ldr     x5, [%1, %4]\n"
        "mov     sp, x5\n"
        
        // Offset GuestContext pointer to the host member
        "add     x5, %1, %5\n"
        
        // Save original host sp and tpidr_el0 to host context
        "stp     x3, x4, [x5, %6]\n"
        
        // Save all callee-saved host GPRs
        "stp     x19, x20, [x5, #0x0]\n"
        "stp     x21, x22, [x5, #0x10]\n"
        "stp     x23, x24, [x5, #0x20]\n"
        "stp     x25, x26, [x5, #0x30]\n"
        "stp     x27, x28, [x5, #0x40]\n"
        "stp     x29, x30, [x5, #0x50]\n"
        
        // Save all callee-saved host FPRs
        "stp     q8, q9,   [x5, #0x0 + %7]\n"
        "stp     q10, q11, [x5, #0x20 + %7]\n"
        "stp     q12, q13, [x5, #0x40 + %7]\n"
        "stp     q14, q15, [x5, #0x60 + %7]\n"
        
        // Load guest tpidr_el0 from argument
        "msr     tpidr_el0, %0\n"
        
        // Tail call the trampoline to restore guest state
        "blr     %2\n"
        
        // Return value will be in x0, store it in result
        "mov     %3, x0\n"
        
        : "+r"(tpidr), "+r"(ctx), "+r"(trampoline_addr), "=r"(result)
        : "i"(GuestContextSp), "i"(GuestContextHostContext), 
          "i"(HostContextSpTpidrEl0), "i"(HostContextVregs)
        : "x3", "x4", "x5", "memory"
    );
    
    return result;
}

HaltReason ArmNce::ReturnToRunCodeByExceptionLevelChange(int tid, void* tpidr) {
    // This function uses syscall to send a signal
    register long x8 asm("x8") = __NR_tkill;
    register long x0 asm("x0") = tid;
    register long x1 asm("x1") = ReturnToRunCodeByExceptionLevelChangeSignal;
    register void* x9 asm("x9") = tpidr;  // Preserve tpidr
    
    asm volatile(
        "svc #0\n"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x9)
        : "memory", "cc"
    );
    
    // Should never reach here, but if it does, return BreakLoop
    return HaltReason::BreakLoop;
}

void ArmNce::ReturnToRunCodeByExceptionLevelChangeSignalHandler(int sig, void* info, void* raw_context) {
    // Call the context restorer
    void* tpidr = RestoreGuestContext(raw_context);
    
    // Get the native context
    auto* params = reinterpret_cast<Kernel::KThread::NativeExecutionParameters*>(tpidr);
    auto* guest_ctx = static_cast<GuestContext*>(params->native_context);
    
    // Save the old value of tpidr_el0
    uint64_t old_tpidr;
    asm volatile("mrs %0, tpidr_el0" : "=r"(old_tpidr));
    
    // Store it in the guest context
    guest_ctx->host_ctx.host_tpidr_el0 = reinterpret_cast<void*>(old_tpidr);
    
    // Set our new tpidr_el0
    asm volatile("msr tpidr_el0, %0" : : "r"(tpidr) : "memory");
    
    // Unlock the context
    UnlockThreadParameters(tpidr);
    
    // Returning from here will enter the guest
}

void ArmNce::BreakFromRunCodeSignalHandler(int sig, void* info, void* raw_context) {
    // Check if we have the correct TLS magic
    uint64_t tpidr_value;
    uint32_t magic_value;
    
    asm volatile(
        "mrs %0, tpidr_el0\n"
        : "=r"(tpidr_value)
        :
        : "memory"
    );
    
    auto* tpidr = reinterpret_cast<Kernel::KThread::NativeExecutionParameters*>(tpidr_value);
    magic_value = tpidr->magic;
    
    if (magic_value != TlsMagic) {
        // Incorrect TLS magic, so this is a spurious signal
        return;
    }
    
    // Correct TLS magic, so this is a guest interrupt
    // Restore host tpidr_el0
    auto* guest_ctx = static_cast<GuestContext*>(tpidr->native_context);
    uint64_t host_tpidr = reinterpret_cast<uint64_t>(guest_ctx->host_ctx.host_tpidr_el0);
    
    asm volatile(
        "msr tpidr_el0, %0\n"
        :
        : "r"(host_tpidr)
        : "memory"
    );
    
    // Save the guest context
    SaveGuestContext(guest_ctx, raw_context);
}

void ArmNce::GuestAlignmentFaultSignalHandler(int sig, void* info, void* raw_context) {
    // Check if we have the correct TLS magic
    uint64_t tpidr_value;
    uint32_t magic_value;
    
    asm volatile(
        "mrs %0, tpidr_el0\n"
        : "=r"(tpidr_value)
        :
        : "memory"
    );
    
    auto* tpidr = reinterpret_cast<Kernel::KThread::NativeExecutionParameters*>(tpidr_value);
    magic_value = tpidr->magic;
    
    if (magic_value != TlsMagic) {
        // Incorrect TLS magic, so this is a host fault
        HandleHostAlignmentFault(sig, info, raw_context);
        return;
    }
    
    // Correct TLS magic, so this is a guest fault
    auto* guest_ctx = static_cast<GuestContext*>(tpidr->native_context);
    uint64_t guest_tpidr = tpidr_value;
    uint64_t host_tpidr = reinterpret_cast<uint64_t>(guest_ctx->host_ctx.host_tpidr_el0);
    
    // Restore host tpidr_el0
    asm volatile(
        "msr tpidr_el0, %0\n"
        :
        : "r"(host_tpidr)
        : "memory"
    );
    
    // Call the handler
    bool restore_guest = HandleGuestAlignmentFault(guest_ctx, info, raw_context);
    
    // If the handler returned true, restore guest tpidr_el0
    if (restore_guest) {
        asm volatile(
            "msr tpidr_el0, %0\n"
            :
            : "r"(guest_tpidr)
            : "memory"
        );
    }
}

void ArmNce::GuestAccessFaultSignalHandler(int sig, void* info, void* raw_context) {
    // Check if we have the correct TLS magic
    uint64_t tpidr_value;
    uint32_t magic_value;
    
    asm volatile(
        "mrs %0, tpidr_el0\n"
        : "=r"(tpidr_value)
        :
        : "memory"
    );
    
    auto* tpidr = reinterpret_cast<Kernel::KThread::NativeExecutionParameters*>(tpidr_value);
    magic_value = tpidr->magic;
    
    if (magic_value != TlsMagic) {
        // Incorrect TLS magic, so this is a host fault
        HandleHostAccessFault(sig, info, raw_context);
        return;
    }
    
    // Correct TLS magic, so this is a guest fault
    auto* guest_ctx = static_cast<GuestContext*>(tpidr->native_context);
    uint64_t guest_tpidr = tpidr_value;
    uint64_t host_tpidr = reinterpret_cast<uint64_t>(guest_ctx->host_ctx.host_tpidr_el0);
    
    // Restore host tpidr_el0
    asm volatile(
        "msr tpidr_el0, %0\n"
        :
        : "r"(host_tpidr)
        : "memory"
    );
    
    // Call the handler
    bool restore_guest = HandleGuestAccessFault(guest_ctx, info, raw_context);
    
    // If the handler returned true, restore guest tpidr_el0
    if (restore_guest) {
        asm volatile(
            "msr tpidr_el0, %0\n"
            :
            : "r"(guest_tpidr)
            : "memory"
        );
    }
}

void ArmNce::LockThreadParameters(void* tpidr) {
    auto* lock_addr = reinterpret_cast<std::atomic<uint32_t>*>(
        reinterpret_cast<uintptr_t>(tpidr) + TpidrEl0Lock
    );
    
    // Implementation of spinlock acquire with load-linked/store-conditional
    uint32_t expected = SpinLockUnlocked;
    uint32_t desired = SpinLockLocked;
    
    while (!lock_addr->compare_exchange_strong(expected, desired, 
                                              std::memory_order_acquire,
                                              std::memory_order_relaxed)) {
        // Reset expected value for next iteration
        expected = SpinLockUnlocked;
        
        // Small pause to reduce contention
        asm volatile("yield" ::: "memory");
    }
}

void ArmNce::UnlockThreadParameters(void* tpidr) {
    auto* lock_addr = reinterpret_cast<std::atomic<uint32_t>*>(
        reinterpret_cast<uintptr_t>(tpidr) + TpidrEl0Lock
    );
    
    // Release the lock with release semantics
    lock_addr->store(SpinLockUnlocked, std::memory_order_release);
}

} // namespace Core

