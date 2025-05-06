CC=g++
CXXFLAGS=-Wall -Wextra -std=c++17

all: server subscriber

server: server.cpp utils.cpp

subscriber: subscriber.cpp utils.cpp

.PHONY clean:
	rm -f server subscriber