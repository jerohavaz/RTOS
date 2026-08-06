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

add_subdirectory(cmake/stm32cubemx)
add_subdirectory(Drivers/BSP)
add_subdirectory(Port/CortexM)
add_subdirectory(RTOS)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    App/Src/app_tasks.c
    App/Src/app.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Inc
    App/Inc
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    rtos_kernel
    bsp
)

list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)