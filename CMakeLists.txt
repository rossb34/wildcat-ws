cmake_minimum_required(VERSION 3.17)
project(wildcat_ws)

set(CMAKE_CXX_STANDARD 20)

set(LIB_NAME wildcat_ws)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_library(wildcat_ws include/wildcat/ws/client.hpp include/wildcat/ws/handshake.hpp)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

# define include directories for the library
target_include_directories( "${LIB_NAME}"
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        )

target_link_libraries(${LIB_NAME} INTERFACE ${CONAN_LIBS})

# Set linker language property to CXX
set_target_properties(${LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)

enable_testing()
add_subdirectory(test)
