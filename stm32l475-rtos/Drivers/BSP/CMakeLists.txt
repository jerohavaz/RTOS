target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/B-L475E-IOT01/stm32l475e_iot01.c
    ${CMAKE_CURRENT_LIST_DIR}/B-L475E-IOT01/stm32l475e_iot01_accelero.c
    ${CMAKE_CURRENT_LIST_DIR}/B-L475E-IOT01/stm32l475e_iot01_gyro.c
    ${CMAKE_CURRENT_LIST_DIR}/Components/lsm6dsl/lsm6dsl.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/B-L475E-IOT01
    ${CMAKE_CURRENT_LIST_DIR}/Components/Common
    ${CMAKE_CURRENT_LIST_DIR}/Components/lsm6dsl
)