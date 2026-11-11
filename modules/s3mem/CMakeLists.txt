add_library(usermod_s3mem INTERFACE)
target_sources(usermod_s3mem INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/mod_s3mem.c
)
target_include_directories(usermod_s3mem INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(usermod INTERFACE usermod_s3mem)

