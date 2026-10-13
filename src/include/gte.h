#ifndef GTE_H
#define GTE_H

/* GTE (geometry engine) command macros. Each command is its raw coprocessor encoding. */

#include <inline_c.h>

#define SDK_GTE_COMMAND(word) __asm__ volatile("nop;" "nop;" ".word " #word)
/* The `_b` forms: the same command without the two leading nops. */
#define SDK_GTE_COMMAND_B(word) __asm__ volatile(".word " #word)

#undef gte_rtps
#define gte_rtps()  SDK_GTE_COMMAND(0x4A180001)
#undef gte_rtpt
#define gte_rtpt()  SDK_GTE_COMMAND(0x4A280030)
#undef gte_nclip
#define gte_nclip() SDK_GTE_COMMAND(0x4B400006)
#undef gte_rtpt_b
#define gte_rtpt_b()  SDK_GTE_COMMAND_B(0x4A280030)
#undef gte_nclip_b
#define gte_nclip_b() SDK_GTE_COMMAND_B(0x4B400006)
#undef gte_avsz3
#define gte_avsz3() SDK_GTE_COMMAND(0x4B58002D)
#undef gte_avsz4
#define gte_avsz4() SDK_GTE_COMMAND(0x4B68002E)
#undef gte_avsz3_b
#define gte_avsz3_b() SDK_GTE_COMMAND_B(0x4B58002D)
#undef gte_avsz4_b
#define gte_avsz4_b() SDK_GTE_COMMAND_B(0x4B68002E)
#undef gte_sqr0
#define gte_sqr0()  SDK_GTE_COMMAND(0x4AA00428)
#undef gte_sqr0_b
#define gte_sqr0_b() SDK_GTE_COMMAND_B(0x4AA00428)
#undef gte_op12
#define gte_op12()  SDK_GTE_COMMAND(0x4B78000C)
#undef gte_rtv0
#define gte_rtv0()  SDK_GTE_COMMAND(0x4A480012)
#undef gte_rtir
#define gte_rtir()  SDK_GTE_COMMAND(0x4A49E012)

#endif
