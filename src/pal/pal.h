#pragma once

#include "pal_consts.h"

// If simultating OE, then we need the underlying platform
#if defined(WASM_ENV)
#  include "pal_wasi.h"
#endif
#if defined(OPEN_ENCLAVE)
#  include "pal_open_enclave.h"
#endif
/*
#if !defined(OPEN_ENCLAVE) || defined(OPEN_ENCLAVE_SIMULATION)
#  include "pal_apple.h"
#  include "pal_freebsd.h"
#  include "pal_freebsd_kernel.h"
#  include "pal_haiku.h"
#  include "pal_linux.h"
#  include "pal_netbsd.h"
#  include "pal_openbsd.h"
#  include "pal_windows.h"
#endif*/
#include "pal_plain.h"

namespace snmalloc
{
  /*
#if !defined(WASM_ENV) || defined(OPEN_ENCLAVE_SIMULATION)
  using DefaultPal =
#  if defined(_WIN32)
    PALWindows;
#  elif defined(__APPLE__)
    PALApple<>;
#  elif defined(__linux__)
    PALLinux;
#  elif defined(FreeBSD_KERNEL)
    PALFreeBSDKernel;
#  elif defined(__FreeBSD__)
    PALFreeBSD;
#  elif defined(__NetBSD__)
    PALNetBSD;
#  elif defined(__OpenBSD__)
    PALOpenBSD;
#  else
#    error Unsupported platform
#  endif
#endif
*/

  using Pal =
#if defined(SNMALLOC_MEMORY_PROVIDER)
    PALPlainMixin<SNMALLOC_MEMORY_PROVIDER>;
#elif defined(WASM_ENV)
    PALPlainMixin<PALWASI>;    
#elif defined(OPEN_ENCLAVE)
    PALPlainMixin<PALOpenEnclave>;
#endif

  [[noreturn]] SNMALLOC_SLOW_PATH inline void error(const char* const str)
  {
    Pal::error(str);
  }

  /**
   * Query whether the PAL supports a specific feature.
   */
  template<PalFeatures F, typename PAL = Pal>
  constexpr static bool pal_supports = (PAL::pal_features & F) == F;

  // Used to keep Superslab metadata committed.
  static constexpr size_t OS_PAGE_SIZE = Pal::page_size;

  static_assert(
    bits::next_pow2_const(OS_PAGE_SIZE) == OS_PAGE_SIZE,
    "OS_PAGE_SIZE must be a power of two");
  /*static_assert(
    OS_PAGE_SIZE % Aal::smallest_page_size == 0,
    "The smallest architectural page size must divide OS_PAGE_SIZE");
*/
  // Some system headers (e.g. Linux' sys/user.h, FreeBSD's machine/param.h)
  // define `PAGE_SIZE` as a macro.  We don't use `PAGE_SIZE` as our variable
  // name, to avoid conflicts, but if we do see a macro definition then check
  // that our value matches the platform's expected value.
#ifdef PAGE_SIZE
  static_assert(
    PAGE_SIZE == OS_PAGE_SIZE,
    "Page size from system header does not match snmalloc config page size.");
#endif

} // namespace snmalloc
