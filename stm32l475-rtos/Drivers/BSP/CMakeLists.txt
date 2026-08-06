add_library(bsp STATIC)

target_sources(bsp PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/B-L475E-IOT01/Src/stm32l475e_iot01.c
    ${CMAKE_CURRENT_LIST_DIR}/B-L475E-IOT01/Src/stm32l475e_iot01_accelero.c
    ${CMAKE_CURRENT_LIST_DIR}/B-L475E-IOT01/Src/stm32l475e_iot01_gyro.c
    ${CMAKE_CURRENT_LIST_DIR}/Components/lsm6dsl/lsm6dsl.c
)

target_include_directories(bsp PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/B-L475E-IOT01/Inc
    ${CMAKE_CURRENT_LIST_DIR}/Components/Common
    ${CMAKE_CURRENT_LIST_DIR}/Components/lsm6dsl
    ${CMAKE_SOURCE_DIR}/Drivers/STM32L4xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32L4xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)

target_link_libraries(bsp PUBLIC
    stm32cubemx
)