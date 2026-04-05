CC=g++
CXXFLAGS=-Wall -Wextra -std=c++17 -O2

all: server subscriber

server: server.cpp TopicTrie.cpp utils.cpp 

subscriber: subscriber.cpp utils.cpp

.PHONY clean:
	rm -f server subscriber