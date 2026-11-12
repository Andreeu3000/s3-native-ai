add_library(usermod_s3img INTERFACE)

target_sources(usermod_s3img INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/mod_s3img.c
)

target_include_directories(usermod_s3img INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod INTERFACE usermod_s3img)
