# Top-level micropython.cmake (patrón aditivo oficial)
add_library(usermod INTERFACE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/modules/s3sys)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/modules/s3mem)
