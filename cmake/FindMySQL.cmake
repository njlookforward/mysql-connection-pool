# cmake/FindMySQL.cmake
# 我理解这是跨平台的配置，MySQL在不同的平台有不同的位置
# 自定义模块可以提供更好的兼容性

# 查找MySQL路径

# 查找头文件路径
find_path(MYSQL_INCLUDE_DIR
    NAMES mysql.h
    PATHS
        /usr/include/mysql
        /usr/local/include/mysql
        /opt/mysql/include/mysql
)

# 查找库文件
find_library(MYSQL_LIBRARY
    NAMES mysqlclient
    PATHS
        /usr/lib
        /usr/lib/mysql
        /usr/local/lib
        /usr/local/lib/mysql
        /opt/mysql/lib
)

# 处理找到的结果
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    MySQL
    REQUIRED_VARS MYSQL_LIBRARY MYSQL_INCLUDE_DIR
)

# find_package_handle_standard_args(
#     MySQL
#     DEFAULT_MSG MYSQL_LIBRARY MYSQL_INCLUDE_DIR
# )
# REQUIRED_VARS是3.17+之后推荐的新写法，显示指定必需变量，会检查需要的变量是否存在，如果失败，会显示标准错误信息
# DEFAULT_MSG是旧写法

# 设置环境变量
if(MySQL_FOUND)
    set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR})
    set(MYSQL_LIBRARIES ${MYSQL_LIBRARY})
endif()

# 标记为高级变量
mark_as_advanced(MYSQL_INCLUDE_DIR MYSQL_LIBRARY)