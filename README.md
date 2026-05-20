# RTOS

## Format
```bash
find Core RTOS App -type f \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 clang-format -i
```