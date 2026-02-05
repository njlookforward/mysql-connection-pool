.PHONY: all clean rebuild run

all:
	@mkdir -p build && cd build && cmake .. && make

clean:
	rm -rf build

rebuild: clean all

run: all
	# ./bin/test_connection_pool
	# ./bin/test_day2_connection
	# ./bin/test_day3_connection
	# 一共需要修改test/CMakeLists.txt与Makefile文件
	./bin/test_day4_connection
	# ./bin/test_future
