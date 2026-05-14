cmake_minimum_required(VERSION 3.10)
project(MetalBallSim)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

include_directories(include)

add_executable(MetalBallSim src/main.cpp src/glad.c)
target_link_libraries(MetalBallSim OpenGL::GL glfw)

if(APPLE)
    target_compile_definitions(MetalBallSim PRIVATE GL_SILENCE_DEPRECATION)
endif()
