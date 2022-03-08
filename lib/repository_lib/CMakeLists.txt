cmake_minimum_required(VERSION 3.1x)

set(PROJECT_NAME repository_lib)
set(LIBRARY_NAME repository_lib)

connect_lib(${LIBRARY_NAME} ${PROJECT_NAME})

target_link_libraries(${LIBRARY_NAME} -lpqxx -lpq)