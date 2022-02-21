cmake_minimum_required(VERSION 3.1x)

set(PROJECT_NAME proxy_client_lib)
set(LIBRARY_NAME proxy_client_lib)

connect_lib(${LIBRARY_NAME} ${PROJECT_NAME})

find_package(OpenSSL REQUIRED)
target_link_libraries(${LIBRARY_NAME} OpenSSL::SSL)