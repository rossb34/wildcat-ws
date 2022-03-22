
add_executable(handshake_tests src/handshake_tests.cpp)
target_link_libraries(handshake_tests ${LIB_NAME} ${CONAN_LIBS})
add_test(NAME run_handshake_tests COMMAND handshake_tests)

add_executable(client_tests src/client_tests.cpp)
target_link_libraries(client_tests ${LIB_NAME} ${CONAN_LIBS})
add_test(NAME run_client_tests COMMAND client_tests)
