.PHONY: all clean rebuild run

all:
	@mkdir -p build && cd build && cmake .. && make

clean:
	rm -rf build

rebuild: clean all

run: all
	# ./bin/test_connection_pool
	./bin/test_day2_connection
