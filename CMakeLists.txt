cmake_minimum_required(VERSION 3.10)
project(HailoCameraApp)

set(CMAKE_CXX_STANDARD 17) # HailoRT 推荐使用 C++17 及以上
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 查找 OpenCV
find_package(OpenCV REQUIRED)

# 包含 HailoRT 头文件路径 (通常通过 apt 安装在系统目录下)
include_directories(/usr/include )

# 添加可执行文件
add_executable(train train.cpp ./src/Myhailo.cpp)

# 链接 OpenCV 和 HailoRT 核心动态库
target_link_libraries(train
    ${OpenCV_LIBS}
    hailort
)