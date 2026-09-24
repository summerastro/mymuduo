cmake_minimum_required(VERSION 2.5)
project(mymuduo)

# mymuo 最终编译成so动态库，设置动态库路径,放在根目录的 lib 文件夹
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
#设置调试信息 以及 启动c++11语言标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

# 定义参与编译的源代码文件，.指代当前目录 这个也就是当前目录下的所有文件
aux_source_directory(. SRC_LIST)
#编译动态库mymuduo
add_library(mymuduo SHARED ${SRC_LIST})
