/**
 * @brief 测试：大缓冲区的 write() 是否原子
 * POSIX 只保证 PIPE_BUF 大小内的写入对管道是原子的
 * 对普通文件没有这样的保证！
 */

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

// 创建一个非常长的字符串（远超 PIPE_BUF）
void large_write_test(const std::string& filename, const char marker) {
    int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("open failed");
        return;
    }

    // 创建一个很大的缓冲区（10KB）
    const int BUF_SIZE = 10240;
    char* buffer = new char[BUF_SIZE];

    for (int i = 0; i < 5; i++) {
        // 填充缓冲区
        memset(buffer, marker, BUF_SIZE);
        int offset = snprintf(buffer, BUF_SIZE,
                             "=== 开始标记 %c_%d ===\n", marker, i);
        memset(buffer + offset, marker, BUF_SIZE / 2);
        offset += BUF_SIZE / 2;
        snprintf(buffer + offset, BUF_SIZE - offset,
                "=== 结束标记 %c_%d ===\n", marker, i);

        // 一次性写入 10KB - 这会被打断吗？
        write(fd, buffer, strlen(buffer));
        fsync(fd);  // 强制刷新到磁盘
    }

    delete[] buffer;
    close(fd);
}

int main() {
    std::string filename = "large_write_test.log";

    // 清空文件
    std::ofstream(filename, std::ios::trunc).close();

    std::cout << "测试：两个线程各写 10KB 的大块数据\n";
    std::cout << "如果 write() 是原子的，我们应该看到完整的 AAA... 和 BBB... 块\n\n";

    std::thread t1(large_write_test, filename, 'A');
    std::thread t2(large_write_test, filename, 'B');

    t1.join();
    t2.join();

    // 分析结果
    std::ifstream result(filename);
    std::string line;
    int total_lines = 0;
    int mixed_lines = 0;

    while (std::getline(result, line)) {
        total_lines++;
        if (line.find("开始标记") != std::string::npos) {
            bool has_A = line.find('A') != std::string::npos;
            bool has_B = line.find('B') != std::string::npos;
            if (has_A && has_B) {
                mixed_lines++;
                std::cout << "⚠️  混合行: " << line << "\n";
            }
        }
    }

    std::cout << "\n总共 " << total_lines << " 行，其中 " << mixed_lines << " 行混合\n";

    return 0;
}
