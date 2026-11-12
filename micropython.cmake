# NO creamos 'usermod' aquí, MicroPython ya lo crea internamente.
# Simplemente añadimos nuestros módulos y los vinculamos a él.

# El segundo argumento especifica el directorio binario para evitar el error de "out-of-tree"
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/modules/s3sys ${CMAKE_CURRENT_BINARY_DIR}/s3sys)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/modules/s3mem ${CMAKE_CURRENT_BINARY_DIR}/s3mem)
