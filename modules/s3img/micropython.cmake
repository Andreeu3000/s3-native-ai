add_library(usermod_s3img INTERFACE)

target_sources(usermod_s3img INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/mod_s3img.c
    ${CMAKE_CURRENT_LIST_DIR}/lodepng/lodepng.c
)

target_include_directories(usermod_s3img INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/lodepng
)

target_compile_definitions(usermod_s3img INTERFACE
    LODEPNG_NO_COMPILE_DISK
    LODEPNG_NO_COMPILE_CPP
)

target_link_libraries(usermod INTERFACE usermod_s3img)
