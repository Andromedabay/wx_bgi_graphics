#pragma once

/**
 * @file openlb_msvc_compat.h
 * @brief Minimal MSVC compatibility shims for OpenLB headers.
 *
 * OpenLB uses POSIX/GCC built-ins and POSIX macros that are not available when
 * compiling with the Microsoft Visual C++ compiler.  This header provides the minimal
 * set of definitions required to include OpenLB headers under MSVC without modifying
 * the OpenLB source tree.
 *
 * Include this header **before** any OpenLB header (e.g., before `#include <olb.h>`)
 * when targeting MSVC.
 *
 * @note On non-MSVC compilers (GCC, Clang, AppleClang) this header expands to nothing;
 *       it is safe to include unconditionally.
 *
 * @warning Native MSVC + OpenLB builds are not fully supported upstream.  OpenLB 1.6
 *          and later still reference POSIX-only headers such as `<unistd.h>` that
 *          cannot be shimmed here.  Linux/WSL remains the recommended build
 *          environment for OpenLB-coupled targets.
 */

#ifdef _MSC_VER

#ifndef M_PI
/** @brief Mathematical constant π, defined for MSVC which does not provide it by default. */
#define M_PI 3.14159265358979323846
#endif

#ifndef __builtin_unreachable
/**
 * @brief Map GCC's `__builtin_unreachable()` to the equivalent MSVC hint.
 *
 * OpenLB headers use `__builtin_unreachable()` to mark code paths that should never
 * be reached.  Under MSVC, `__assume(0)` communicates the same information to the
 * optimiser.
 */
#define __builtin_unreachable() __assume(0)
#endif

#endif
