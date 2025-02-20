cmake_minimum_required(VERSION 3.10)

project(SocketTest)

file(GLOB SocketTest_SRC
    "SocketTest.cpp"
)

add_executable(SocketTest ${SocketTest_SRC})
