add_library(usermod_s3qr INTERFACE)

target_sources(usermod_s3qr INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/mod_s3qr.c
)

target_include_directories(usermod_s3qr INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod_s3qr INTERFACE idf::espressif__quirc)

target_link_libraries(usermod INTERFACE usermod_s3qr)
