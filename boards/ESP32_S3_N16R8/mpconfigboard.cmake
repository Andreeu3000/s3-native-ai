set(IDF_TARGET esp32s3)
set(SDKCONFIG_DEFAULTS
    boards/sdkconfig.base
    boards/sdkconfig.usb
    boards/sdkconfig.ble
    boards/sdkconfig.spiram_sx
    ${CMAKE_CURRENT_LIST_DIR}/sdkconfig.board
)
set(MICROPY_DEF_BOARD
    MICROPY_HW_BOARD_NAME="ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM Octal)"
)
