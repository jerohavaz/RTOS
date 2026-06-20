add_library(rtos_port STATIC)

target_sources(rtos_port PRIVATE
    Src/port.c
    Src/portasm.s
)

target_include_directories(rtos_port
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/Inc
)

target_link_libraries(rtos_port
    PRIVATE
        rtos_config
        stm32cubemx
)