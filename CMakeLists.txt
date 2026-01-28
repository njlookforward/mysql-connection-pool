# 编辑顶层的主CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 设置我的项目名称，版本，编程语言，错了，直接就是项目名称和版本
project(mysql_connect_pool VERSION 1.0.0 LANGUAGES CXX)

# 添加自定义的模块路径
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# 设置我的C++编译器版本
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项 这里有拼写错误：add_compile_options
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -g)
endif()

# 查找依赖
find_package(Threads REQUIRED)
find_package(MySQL REQUIRED)    # ??? 我们自定义的查找模块

# 添加include目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 可以在顶层CMakeLists.txt中设置可执行程序、库文件的存储路径，这样的话，子CMakeLists.txt可以继承
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
# set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
# set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

# 设置默认生成的是动态库文件，在顶层CMakeLists.txt中进行全局设置，影响所有的add_library
set(BUILD_SHARED_LIBS ON)

# 创建库目标
add_subdirectory(src)

# 创建测试目标
add_subdirectory(test)

# 安装规则
install(DIRECTORY include/ DESTINATION include) 

# 打印配置信息
message(STATUS "Project name: ${PROJECT_NAME}")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "MySQL Include: ${MYSQL_INCLUDE_DIRS}")
message(STATUS "MySQL Library: ${MYSQL_LIBRARIES}")