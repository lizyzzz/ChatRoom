cmake_minimum_required(VERSION 3.0)


project(network)

add_compile_options(-g -std=c++14)

include_directories(./include)

# 生成动态链接库
add_library(cppNetWork SHARED src/cppNetWork.cpp)

add_executable(chatRoomServer src/chatRoomServer.cpp)
target_link_libraries(chatRoomServer 
    pthread
    cppNetWork
)

add_executable(chatRoomClient src/chatRoomClient.cpp)
target_link_libraries(chatRoomClient 
    pthread
    cppNetWork
)
