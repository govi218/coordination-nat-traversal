CC := clang++
CXXFLAGS = -g -Wall

all: clean peer

peer:
	${CC} ${CFLAGS} -o peer peer.cpp
clean:
	rm peer
