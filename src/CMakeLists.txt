# 创建库目标 src/CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 获取所有源文件
file(GLOB SOURCES "*.cpp")

# 创建库
add_library(dbconnectionpool SHARED ${SOURCES})

# 设置生成目标所包含的目录
# BUG 对于继承属性，我应该使用大写PUBLIC，而不是小写字母
target_include_directories(dbconnectionpool
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${MYSQL_INCLUDE_DIRS}      # 这是包含mysql.h的头文件路径    
)

# 链接依赖库
# BUG 我又把PUBLIC写成了public，因为是小写，所以链接库的目标又多了一个libpublic
# public小写被当成库名，链接器回去寻找libpublic.so，但是找不到，发生ld链接错误
target_link_libraries(dbconnectionpool
    PUBLIC
        ${MYSQL_LIBRARIES}      # libmysqlclinet
        Threads::Threads        # thread
)

# 设置库属性，这是什么意思，不懂
# 这里缺少了PROPERTIES properties
set_target_properties(dbconnectionpool PROPERTIES
    VERSION ${PROJECT_VERSION}          # 精确版本号
    SOVERSION ${PROJECT_VERSION_MAJOR}  # 主版本号，这是ABI版本，是一个符号链接，指向真正地动态库实现文件
    POSITION_INDEPENDENT_CODE ON        # 生成PIC（位置无关代码），这是生成动态库必须指定的属性
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib    # 设置动态库的输出目录
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib    # 设置静态哭的输出目录
)

# 目标的安装规则
install(TARGETS dbconnectionpool
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
