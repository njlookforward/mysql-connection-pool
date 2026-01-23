# 这是直接生成可执行文件，比库文件的设置逻辑要少一些
# 我需要自己指定输出的可执行文件的路径
cmake_minimum_required(VERSION 3.10)

# 生成的可执行文件，以及需要包含的源文件集合
add_executable(test_connection_pool test_basic1.cpp)

# 指定生成的可执行文件目标的存储路径
set_target_properties(test_connection_pool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

# 指定目标的需要包含的头文件路径
target_include_directories(test_connection_pool
    PRIVATE 
        ${CMAKE_SOURCE_DIR}/include 
)

# 指定生成可执行程序时所需要的库文件的集合
target_link_libraries(test_connection_pool
    PRIVATE 
        dbconnectionpool
)
