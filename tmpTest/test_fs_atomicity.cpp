/**
 * @brief 测试：两个独立的进程写同一个文件
 * 验证操作系统层面的原子性保证
 *
 * 编译：g++ test_fs_atomicity.cpp -o test_fs
 * 运行：./test_fs &  ./test_fs &  wait
 */

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <random>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// 使用底层系统调用直接写文件
void system_call_writer(const std::string& filename, char marker) {
    int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("open failed");
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());

    for (int i = 0; i < 20; i++) {
        char buffer[128];
        int len = snprintf(buffer, sizeof(buffer),
                          "%c_%05d: 这是一个相对较长的字符串，用于测试原子性\n",
                          marker, i);

        // 系统调用 write() - 这是否原子？
        ssize_t written = write(fd, buffer, len);

        // 随机延迟，增加交错概率
        std::this_thread::sleep_for(std::chrono::microseconds(gen() % 1000));
    }

    close(fd);
}

// 使用 C++ 流写文件
void cpp_stream_writer(const std::string& filename, char marker) {
    std::ofstream file(filename, std::ios::app);

    std::random_device rd;
    std::mt19937 gen(rd());

    for (int i = 0; i < 20; i++) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer),
                "%c_%05d: C++流写入的字符串，看看是否原子\n",
                marker, i);

        file << buffer;
        file.flush();

        std::this_thread::sleep_for(std::chrono::microseconds(gen() % 1000));
    }
}

int main(int argc, char* argv[]) {
    std::string filename = "atomicity_test.log";

    // 清空文件
    std::ofstream(filename, std::ios::trunc).close();

    std::cout << "测试：两个线程写同一个文件\n";

    // 测试1：使用系统调用
    std::cout << "\n=== 测试1: 系统调用 write() ===\n";
    std::thread t1(system_call_writer, filename, 'A');
    std::thread t2(system_call_writer, filename, 'B');

    t1.join();
    t2.join();

    // 读取并检查结果
    std::ifstream result(filename);
    std::string line;
    int corrupted = 0;
    int total = 0;

    while (std::getline(result, line)) {
        total++;
        // 检查是否有 A 和 B 混在同一行
        bool has_A = line.find("A_") == 0;
        bool has_B = line.find("B_") == 0;

        if (!has_A && !has_B) {
            // 可能是混合行
            if (line.find('A') != std::string::npos && line.find('B') != std::string::npos) {
                corrupted++;
                std::cout << "⚠️  混乱行: " << line << "\n";
            }
        }
    }

    std::cout << "\n总行数: " << total << ", 混乱行: " << corrupted << "\n";

    // 测试2：使用 O_SYNC 强制同步写入，强制同步写入有什么意义呢？
    filename = "atomicity_test_sync.log";
    std::ofstream(filename, std::ios::trunc).close();

    std::cout << "\n=== 测试2: O_SYNC (同步写入) ===\n";

    std::thread t3([&filename]() {
        int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_SYNC, 0644);
        for (int i = 0; i < 10; i++) {
            char buffer[64];
            int len = snprintf(buffer, sizeof(buffer), "X_%03d\n", i);
            write(fd, buffer, len);
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
        close(fd);
    });

    std::thread t4([&filename]() {
        int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_SYNC, 0644);
        for (int i = 0; i < 10; i++) {
            char buffer[64];
            int len = snprintf(buffer, sizeof(buffer), "Y_%03d\n", i);
            write(fd, buffer, len);
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
        close(fd);
    });

    t3.join();
    t4.join();

    std::ifstream result2(filename);
    std::cout << "结果:\n";
    while (std::getline(result2, line)) {
        std::cout << line << "\n";
    }

    // 测试3：使用C++流
    filename = "atomicity_test.log";

    // 清空文件
    std::ofstream(filename, std::ios::trunc).close();

    std::cout << "\n=== 测试3: C++ ofstream ===\n";
    std::thread t5(cpp_stream_writer, filename, 'A');
    std::thread t6(cpp_stream_writer, filename, 'B');

    t5.join();
    t6.join();

    // 读取并检查结果
    std::ifstream result3(filename);
    corrupted = 0;
    total = 0;

    while (std::getline(result3, line)) {
        total++;
        // 检查是否有 A 和 B 混在同一行
        bool has_A = line.find("A_") == 0;
        bool has_B = line.find("B_") == 0;

        if (!has_A && !has_B) {
            // 可能是混合行
            if (line.find('A') != std::string::npos && line.find('B') != std::string::npos) {
                corrupted++;
                std::cout << "⚠️  混乱行: " << line << "\n";
            }
        }
    }

    std::cout << "\n总行数: " << total << ", 混乱行: " << corrupted << "\n";

    return 0;
}
