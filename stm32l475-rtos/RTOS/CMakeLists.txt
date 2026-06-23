add_library(rtos_kernel STATIC)

target_sources(rtos_kernel PRIVATE
    Internal/Src/kernel_panic.c
    Internal/Src/prio_waitq.c
    Internal/Src/task_list.c
    Internal/Src/timeout_list.c
    Internal/Src/trace.c

    Kernel/Src/k_idle.c
    Kernel/Src/k_sched.c
    Kernel/Src/k_task.c
    Kernel/Src/k_timeout.c
    Kernel/Src/os_delay.c
    Kernel/Src/os_isr.c
    Kernel/Src/os_sem.c
    Kernel/Src/os.c

    ../Libs/SEGGER/RTT/RTT/SEGGER_RTT_ASM_ARMv7M.S
    ../Libs/SEGGER/RTT/RTT/SEGGER_RTT_printf.c
    ../Libs/SEGGER/RTT/RTT/SEGGER_RTT.c
    ../Libs/SEGGER/RTT/Syscalls/SEGGER_RTT_Syscalls_GCC.c
    ../Libs/SEGGER/SystemView/SYSVIEW/SEGGER_SYSVIEW.c
    ../Libs/SEGGER/SystemView/Config/SEGGER_SYSVIEW_Config_NoOS.c
)

target_include_directories(rtos_kernel
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/Public
        ${CMAKE_CURRENT_LIST_DIR}/Internal/Inc

    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/Kernel/Inc

        ${CMAKE_CURRENT_LIST_DIR}/../Libs/SEGGER/SystemView/Config
        ${CMAKE_CURRENT_LIST_DIR}/../Libs/SEGGER/SystemView/SEGGER
        ${CMAKE_CURRENT_LIST_DIR}/../Libs/SEGGER/SystemView/SYSVIEW
        ${CMAKE_CURRENT_LIST_DIR}/../Libs/SEGGER/RTT/Config
        ${CMAKE_CURRENT_LIST_DIR}/../Libs/SEGGER/RTT/RTT
)

target_link_libraries(rtos_kernel
    PUBLIC
        rtos_config

    PRIVATE
        rtos_port
        stm32cubemx
)