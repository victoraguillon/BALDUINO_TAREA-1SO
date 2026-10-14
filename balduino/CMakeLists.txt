cmake_minimum_required(VERSION 3.16)
project(Balduino VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de compilacion" FORCE)
endif()

add_compile_options(-Wall -Wextra)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(balduino
    src/main.cpp
    src/Utils.cpp
    src/ProcReader.cpp
    src/ProcStats.cpp
    src/ProcActions.cpp
    src/Terminal.cpp
    src/Renderer.cpp
    src/Views.cpp
    src/Input.cpp
    src/App.cpp
)

target_include_directories(balduino PRIVATE src)

target_compile_options(balduino PRIVATE -O2 -pthread)
target_link_options(balduino PRIVATE -pthread)
target_link_libraries(balduino PRIVATE Threads::Threads)
