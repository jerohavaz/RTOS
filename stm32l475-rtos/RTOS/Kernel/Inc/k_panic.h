#ifndef K_PANIC_H_
#define K_PANIC_H_

#if defined(__GNUC__) || defined(__clang__)
#define K_NORETURN      __attribute__((noreturn))
#define K_UNREACHABLE() __builtin_unreachable()
#elif defined(__ICCARM__)
#define K_NORETURN      __noreturn
#define K_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#define K_NORETURN      __declspec(noreturn)
#define K_UNREACHABLE() __assume(0)
#else
#define K_NORETURN
#define K_UNREACHABLE() do { for (;;) { } } while (0)
#endif

K_NORETURN void k_panic(void);

#define K_PANIC()        \
    do {                 \
        k_panic();       \
        K_UNREACHABLE(); \
    } while (0)

#define K_REQUIRE(cond)  \
    do {                 \
        if (!(cond)) {   \
            K_PANIC();   \
        }                \
    } while (0)

#endif /* K_PANIC_H_ */