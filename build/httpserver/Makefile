cc=g++
bin=http_server

.PHONY:all
all:$(bin)

$(bin):./*.cpp ../common/log.cpp
	$(cc) $^ -o $@ -std=c++11 -lpthread

.PHONY:clean
clean:
	-rm -rf $(bin)
