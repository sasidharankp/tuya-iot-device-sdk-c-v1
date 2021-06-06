set( DEVICE_NAME "farmostat" )

# Demo target.
add_executable(
    ${DEVICE_NAME}
        "${DEVICE_NAME}.c"
        "qrencode_print.c"
)

target_link_libraries(
    ${DEVICE_NAME}
    PUBLIC
        link_core
        qrencode
        wiringPi
)

target_include_directories(
    ${DEVICE_NAME}
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)
