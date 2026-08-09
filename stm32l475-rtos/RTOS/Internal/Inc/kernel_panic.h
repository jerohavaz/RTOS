/**
 * @file kernel_panic.h
 * @brief Internal fatal-error and kernel-invariant utilities.
 * @author Jerome
 *
 * @details
 * Provides a compiler-portable non-returning annotation, an unreachable-code
 * hint, the kernel panic entry point, and the invariant-checking macros used
 * throughout the RTOS implementation.
 *
 * A failed invariant is always fatal. Unlike the standard @c assert macro,
 * @ref KERNEL_REQUIRE is not removed by defining @c NDEBUG.
 */

#ifndef KERNEL_PANIC_H_
#define KERNEL_PANIC_H_

/**
 * @def KERNEL_NORETURN
 * @brief Compiler-specific annotation for functions that never return.
 *
 * Expands to the supported non-returning function attribute for GCC, Clang,
 * IAR, or Microsoft C. It expands to nothing for an unknown compiler.
 */

/**
 * @def KERNEL_UNREACHABLE
 * @brief Mark the following control-flow path as unreachable.
 *
 * Uses the compiler's unreachable intrinsic when available. The generic
 * fallback enters an infinite loop, preserving the non-returning behavior
 * without relying on a compiler extension.
 *
 * @warning This macro may be reached only after a function that is guaranteed
 *          not to return. Executing a compiler unreachable intrinsic on a
 *          reachable path produces undefined behavior.
 */
#if defined(__GNUC__) || defined(__clang__)
#define KERNEL_NORETURN      __attribute__((noreturn))
#define KERNEL_UNREACHABLE() __builtin_unreachable()
#elif defined(__ICCARM__)
#define KERNEL_NORETURN      __noreturn
#define KERNEL_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#define KERNEL_NORETURN      __declspec(noreturn)
#define KERNEL_UNREACHABLE() __assume(0)
#else
#define KERNEL_NORETURN
#define KERNEL_UNREACHABLE() \
    do {                     \
        for (;;) {           \
        }                    \
    } while (0)
#endif

/**
 * @brief Enter the kernel's non-returning fatal-error path.
 *
 * The current implementation calls @c port_halt(), which disables interrupts
 * and enters the port's permanent debug halt loop.
 *
 * @pre The port layer must be initialized sufficiently for @c port_halt() to
 *      execute safely.
 * @post Normal program execution cannot continue.
 */
KERNEL_NORETURN void kernel_panic(void);

/**
 * @def KERNEL_PANIC
 * @brief Terminate execution because of an unrecoverable kernel error.
 *
 * Calls @ref kernel_panic and then marks the remaining path unreachable for
 * control-flow analysis and optimization.
 *
 * The @c do-while wrapper makes the macro behave as one statement in all
 * conditional contexts.
 */
#define KERNEL_PANIC()        \
    do {                      \
        kernel_panic();       \
        KERNEL_UNREACHABLE(); \
    } while (0)

/**
 * @def KERNEL_REQUIRE
 * @brief Enforce an internal kernel invariant.
 *
 * Evaluates @p cond exactly once. If it evaluates to false, execution enters
 * @ref KERNEL_PANIC and never returns.
 *
 * @param cond Boolean expression that must evaluate to true.
 *
 * @warning This is a fatal invariant check, not recoverable argument
 *          validation. Public API errors should normally be reported through
 *          an @c os_status_t result instead.
 * @warning Avoid side effects in @p cond even though the current macro
 *          evaluates it once.
 */
#define KERNEL_REQUIRE(cond) \
    do {                     \
        if (!(cond)) {       \
            KERNEL_PANIC();  \
        }                    \
    } while (0)

#endif /* KERNEL_PANIC_H_ */