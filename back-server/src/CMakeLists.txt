add_executable(testing1 main.cpp)


#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/Crow)
target_link_libraries(testing1 PRIVATE OpenSSL::SSL OpenSSL::Crypto)

set(SERVER_KEYS_SRC "${CMAKE_SOURCE_DIR}/serverkeys")
set(SERVER_KEYS_DEST "${CMAKE_CURRENT_BINARY_DIR}/serverkeys")

add_custom_command(
    TARGET testing1 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SERVER_KEYS_SRC}
            ${SERVER_KEYS_DEST}
    COMMENT "Copying server keys to build directory..."
)
