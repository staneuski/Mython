cmake_minimum_required(VERSION 3.13)

project(Mython)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror)

set(SRC_DIR ./)
aux_source_directory(${SRC_DIR} SRC)

add_executable(main ${SRC})