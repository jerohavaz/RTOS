cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_PROJECT_NAME rtos)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

enable_language(C ASM)

add_executable(${CMAKE_PROJECT_NAME})

add_library(rtos_config INTERFACE)

target_include_directories(rtos_config INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/Config
)

# ---------------------------------------------------------------------------
# CI/build-time configuration overrides
# ---------------------------------------------------------------------------

set(RTOS_PROJECT "" CACHE STRING
    "Integration-test project selected at compile time")

set(RTOS_TRACE_ENABLED "" CACHE STRING
    "Override OS_TRACE_ENABLED (0 or 1)")

if(NOT RTOS_PROJECT STREQUAL "")
    target_compile_definitions(rtos_config INTERFACE
        PROJECT=${RTOS_PROJECT}
    )
endif()

if(NOT RTOS_TRACE_ENABLED STREQUAL "")
    target_compile_definitions(rtos_config INTERFACE
        OS_TRACE_ENABLED=${RTOS_TRACE_ENABLED}
    )
endif()

add_subdirectory(cmake/stm32cubemx)
add_subdirectory(Drivers/BSP)
add_subdirectory(Port/CortexM)
add_subdirectory(RTOS)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    App/Src/app.c
    App/Src/integration_delay.c
    App/Src/integration_mutex.c
    App/Src/integration_queue.c
    App/Src/integration_scheduler.c
    App/Src/integration_semaphore.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Inc
    App/Inc
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    rtos_kernel
)

list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)