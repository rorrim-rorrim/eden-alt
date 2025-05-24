#pragma once

#include <atomic>
#include <cstdint>
#include <signal.h>
#include <ucontext.h>

#include "core/arm/nce/guest_context.h"
#include "core/arm/nce/arm_nce_asm_definitions.h"

namespace Core {

class ArmNce {
public:
    /**
     * Returns to running guest code via the provided trampoline
     * 
     * @param tpidr Thread-local storage pointer
     * @param ctx Guest context
     * @param trampoline_addr Address of the trampoline function to use
     * @return The halt reason
     */
    static HaltReason ReturnToRunCodeByTrampoline(void* tpidr, GuestContext* ctx, uint64_t trampoline_addr);
    
    /**
     * Returns to running guest code by changing exception level
     * 
     * @param tid Thread ID
     * @param tpidr Thread-local storage pointer
     * @return The halt reason
     */
    static HaltReason ReturnToRunCodeByExceptionLevelChange(int tid, void* tpidr);
    
    /**
     * Signal handler for returning to guest code by exception level change
     */
    static void ReturnToRunCodeByExceptionLevelChangeSignalHandler(int sig, void* info, void* raw_context);
    
    /**
     * Signal handler for breaking from guest code
     */
    static void BreakFromRunCodeSignalHandler(int sig, void* info, void* raw_context);
    
    /**
     * Signal handler for guest alignment faults
     */
    static void GuestAlignmentFaultSignalHandler(int sig, void* info, void* raw_context);
    
    /**
     * Signal handler for guest memory access faults
     */
    static void GuestAccessFaultSignalHandler(int sig, void* info, void* raw_context);
    
    /**
     * Handles guest alignment faults
     */
    static bool HandleGuestAlignmentFault(GuestContext* guest_ctx, void* raw_info, void* raw_context);
    
    /**
     * Handles guest memory access faults
     */
    static bool HandleGuestAccessFault(GuestContext* guest_ctx, void* raw_info, void* raw_context);
    
    /**
     * Handles host alignment faults
     */
    static void HandleHostAlignmentFault(int sig, void* raw_info, void* raw_context);
    
    /**
     * Handles host memory access faults
     */
    static void HandleHostAccessFault(int sig, void* raw_info, void* raw_context);
    
    /**
     * Restores the guest context from raw context
     */
    static void* RestoreGuestContext(void* raw_context);
    
    /**
     * Saves the guest context to raw context
     */
    static void SaveGuestContext(GuestContext* guest_ctx, void* raw_context);
    
    /**
     * Locks the thread parameters
     */
    static void LockThreadParameters(void* tpidr);
    
    /**
     * Unlocks the thread parameters
     */
    static void UnlockThreadParameters(void* tpidr);
};

} // namespace Core
