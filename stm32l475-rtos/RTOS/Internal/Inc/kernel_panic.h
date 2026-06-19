#ifndef KERNEL_PANIC_H_
#define KERNEL_PANIC_H_

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

KERNEL_NORETURN void kernel_panic(void);

#define KERNEL_PANIC()        \
    do {                      \
        kernel_panic();       \
        KERNEL_UNREACHABLE(); \
    } while (0)

#define KERNEL_REQUIRE(cond) \
    do {                     \
        if (!(cond)) {       \
            KERNEL_PANIC();  \
        }                    \
    } while (0)

#endif /* KERNEL_PANIC_H_ */