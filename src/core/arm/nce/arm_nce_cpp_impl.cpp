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
        "mov     x3, sp\n"               // Save current stack pointer
        "mrs     x4, tpidr_el0\n"        // Save current thread-local storage pointer
        
        // Load guest sp
        "ldr     x5, [%1, %4]\n"         // Load guest's stack pointer from GuestContext
        "mov     sp, x5\n"               // Switch to guest's stack
        
        // Setup pointer to host context for saving registers
        "add     x5, %1, %5\n"           // Calculate pointer to HostContext
        
        // Save original host sp and tpidr_el0 to host context
        "stp     x3, x4, [x5, %6]\n"     // Store sp and tpidr_el0 pair
        
        // Save all callee-saved host GPRs (x19-x30)
        "stp     x19, x20, [x5, #0x0]\n"  // host_saved_regs[0-1]
        "stp     x21, x22, [x5, #0x10]\n" // host_saved_regs[2-3]
        "stp     x23, x24, [x5, #0x20]\n" // host_saved_regs[4-5]
        "stp     x25, x26, [x5, #0x30]\n" // host_saved_regs[6-7]
        "stp     x27, x28, [x5, #0x40]\n" // host_saved_regs[8-9]
        "stp     x29, x30, [x5, #0x50]\n" // host_saved_regs[10-11] (fp and lr)
        
        // Save all callee-saved host FPRs (q8-q15)
        "stp     q8, q9,   [x5, #0x0 + %7]\n"  // host_saved_vregs[0-1]
        "stp     q10, q11, [x5, #0x20 + %7]\n" // host_saved_vregs[2-3]
        "stp     q12, q13, [x5, #0x40 + %7]\n" // host_saved_vregs[4-5]
        "stp     q14, q15, [x5, #0x60 + %7]\n" // host_saved_vregs[6-7]
        
        // Setup guest context - switch thread-local storage pointer
        "msr     tpidr_el0, %0\n"        // Set tpidr_el0 to the thread parameters
        
        // Call the trampoline which will restore guest registers and return to guest code
        "blr     %2\n"                   // Jump to trampoline function
        
        // When control returns here, save the result
        "mov     %3, x0\n"               // Save halt reason to result
        
        : "+r"(tpidr), "+r"(ctx), "+r"(trampoline_addr), "=r"(result)
        : "i"(GuestContextSp), "i"(GuestContextHostContext), 
          "i"(HostContextSpTpidrEl0), "i"(HostContextVregs)
        : "x3", "x4", "x5", "memory"
    );
    
    return result;
}

HaltReason ArmNce::ReturnToRunCodeByExceptionLevelChange(int tid, void* tpidr) {
    // This function sends a signal to the specified thread using the tkill syscall
    // The thread-local storage pointer is preserved across the syscall
    
    // The original assembly implementation was:
    // register long x8 asm("x8") = __NR_tkill;
    // register long x0 asm("x0") = tid;
    // register long x1 asm("x1") = ReturnToRunCodeByExceptionLevelChangeSignal;
    // register void* x9 asm("x9") = tpidr;  // Preserve tpidr
    // asm volatile("svc #0\n" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x9) : "memory", "cc");
    
    // We can achieve the same with a direct syscall in C++
    // Store the tpidr_el0 value so we can restore it after the syscall
    uint64_t current_tpidr;
    asm volatile("mrs %0, tpidr_el0" : "=r"(current_tpidr) :: "memory");
    
    // Call the tkill syscall to send the signal
    syscall(__NR_tkill, tid, ReturnToRunCodeByExceptionLevelChangeSignal);
    
    // Restore tpidr_el0 after the syscall
    asm volatile("msr tpidr_el0, %0" :: "r"(current_tpidr) : "memory");
    
    // Should never reach here (the signal handler should take over),
    // but if it does, return BreakLoop
    return HaltReason::BreakLoop;
}

void ArmNce::ReturnToRunCodeByExceptionLevelChangeSignalHandler(int sig, void* info, void* raw_context) {
    // Call the context restorer
    void* tpidr = RestoreGuestContext(raw_context);
    
    // Get the native context
    auto* params = reinterpret_cast<Kernel::KThread::NativeExecutionParameters*>(tpidr);
    auto* guest_ctx = static_cast<GuestContext*>(params->native_context);
    
    // Save the old value of tpidr_el0
    uintptr_t old_tpidr;
    asm volatile("mrs %0, tpidr_el0" : "=r"(old_tpidr));
    
    // Store it in the guest context
    guest_ctx->host_ctx.host_tpidr_el0 = reinterpret_cast<void*>(old_tpidr);
    
    // Set our new tpidr_el0
    asm volatile("msr tpidr_el0, %0" : : "r"(tpidr));
    
    // Unlock the context
    UnlockThreadParameters(tpidr);
    
    // Returning from here will enter the guest
}

void ArmNce::BreakFromRunCodeSignalHandler(int sig, void* raw_info, void* raw_context) {
    // Extract the guest context from tpidr_el0
    uint64_t tpidr_el0;
    asm volatile("mrs %0, tpidr_el0" : "=r"(tpidr_el0));
    
    auto* tpidr = reinterpret_cast<Kernel::KThread::NativeExecutionParameters*>(tpidr_el0);
    
    // Check if the magic value is correct
    if (tpidr->magic != TpidrEl0TlsMagic) {
        return;
    }
    
    // Save the guest context and unlock the thread
    auto* guest_ctx = static_cast<GuestContext*>(tpidr->native_context);
    SaveGuestContext(guest_ctx, raw_context);
    UnlockThreadParameters(tpidr);
    
    // Exit from running guest code by marking is_running as false
    tpidr->is_running = false;
}

void ArmNce::GuestAlignmentFaultSignalHandler(int sig, void* raw_info, void* raw_context) {
    // Extract the guest context from tpidr_el0
    uint64_t tpidr_el0;
    asm volatile("mrs %0, tpidr_el0" : "=r"(tpidr_el0));
    
    auto* tpidr = reinterpret_cast<Kernel::KThread::NativeExecutionParameters*>(tpidr_el0);
    
    // Check if the magic value is correct and the context is locked
    if (tpidr->magic != TpidrEl0TlsMagic || tpidr->lock.load(std::memory_order_relaxed) != SpinLockLocked) {
        // Not our context, call original handler
        HandleHostAlignmentFault(sig, raw_info, raw_context);
        return;
    }
    
    auto* guest_ctx = static_cast<GuestContext*>(tpidr->native_context);
    
    // Call the handler and check if we should restore the guest context
    if (HandleGuestAlignmentFault(guest_ctx, raw_info, raw_context)) {
        // Restore the guest context
        tpidr_el0 = reinterpret_cast<uint64_t>(RestoreGuestContext(raw_context));
        asm volatile("msr tpidr_el0, %0" :: "r"(tpidr_el0));
    }
}

void ArmNce::GuestAccessFaultSignalHandler(int sig, void* raw_info, void* raw_context) {
    // Extract the guest context from tpidr_el0
    uint64_t tpidr_el0;
    asm volatile("mrs %0, tpidr_el0" : "=r"(tpidr_el0) :: "memory");
    
    auto* tpidr = reinterpret_cast<Kernel::KThread::NativeExecutionParameters*>(tpidr_el0);
    
    // Check if the magic value is correct and the context is locked
    if (tpidr->magic != TpidrEl0TlsMagic || tpidr->lock.load(std::memory_order_relaxed) != SpinLockLocked) {
        // Not our context, call original handler
        HandleHostAccessFault(sig, raw_info, raw_context);
        return;
    }
    
    auto* guest_ctx = static_cast<GuestContext*>(tpidr->native_context);
    
    // Call the handler and check if we should restore the guest context
    if (HandleGuestAccessFault(guest_ctx, raw_info, raw_context)) {
        // Restore the guest context
        tpidr_el0 = reinterpret_cast<uint64_t>(RestoreGuestContext(raw_context));
        asm volatile("msr tpidr_el0, %0" :: "r"(tpidr_el0) : "memory");
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

