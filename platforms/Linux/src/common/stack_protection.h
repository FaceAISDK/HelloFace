#pragma once

#if defined(__linux__)
#include <features.h>
#if defined(__GLIBC__) && defined(__x86_64__)
// Use the OS failure handler rather than a statically linked compiler fallback.
__asm__(".symver __stack_chk_fail,__stack_chk_fail@GLIBC_2.4");
#elif defined(__GLIBC__) && defined(__aarch64__)
// AArch64 uses a global guard. Both symbols must come from the target libc.
__asm__(".symver __stack_chk_fail,__stack_chk_fail@GLIBC_2.17");
__asm__(".symver __stack_chk_guard,__stack_chk_guard@GLIBC_2.17");
#endif
#endif
