cmake_minimum_required(VERSION 3.10)
project(Test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#查找 Open CV
find_package(OpenCV REQUIRED)


#包含HailoRT头文件路径
include_directories(/usr/include/)
#添加可执行文件
add_executable(Test 
                main.cpp
                src/Myhailo.cpp
                src/open_camera.cpp)

#链接Open CV库和hailort库
target_link_libraries(Test ${OpenCV_LIBS} hailort)

target_include_directories(Test PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include  # 如果有单独的头文件目录
)

