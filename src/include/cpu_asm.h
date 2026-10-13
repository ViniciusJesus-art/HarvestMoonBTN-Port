#ifndef CPU_ASM_H
#define CPU_ASM_H

/* The CPU operations C cannot express: the scratchpad stack, the return-address store
 * and the host-debugger break. */

/* The scratchpad: the PSX's 1 KB of on-chip RAM, much faster than main memory. */
#define SCRATCHPAD          0x1F800000
#define SCRATCHPAD_SIZE     0x400
/* Its last word: the saved main stack pointer; the scratchpad stack grows down from it. */
#define SCRATCHPAD_STACK_TOP (SCRATCHPAD + SCRATCHPAD_SIZE - 4)

/* Run code on the scratchpad stack, so the hottest call-heavy paths keep their
 * stack frames in fast RAM. */
#define SCRATCHPAD_STACK_ENTER()                                        \
    {                                                                   \
        __asm__ volatile("move $8,%0" ::"r"(SCRATCHPAD_STACK_TOP) : "$8", "memory"); \
        __asm__ volatile("sw $sp,0($8)" ::: "$8", "memory");            \
        __asm__ volatile("addiu $8,$8,-4" ::: "$8", "memory");          \
        __asm__ volatile("move $sp,$8" ::: "$8", "memory");             \
    }
#define SCRATCHPAD_STACK_LEAVE()                                        \
    {                                                                   \
        __asm__ volatile("addiu $sp,$sp,4" ::: "$29", "memory");        \
        __asm__ volatile("lw $sp,0($sp)" ::: "$29", "memory");          \
    }

/* Store the caller's return address at `where`. */
#define SAVE_RETURN_ADDRESS(where)                                      \
    {                                                                   \
        __asm__ volatile("move $8,%0" ::"r"(where) : "$8");             \
        __asm__ volatile("sw $31,0($8)" ::: "$8", "memory");            \
    }

/* The SN Systems host-debugger poll; a no-op trap under no debugger. */
#define pollhost() __asm__ volatile("break 1024")

#endif
