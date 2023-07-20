CFLAGS=-Wall -Wextra -Werror -pedantic -std=c++17
all: main.out test.out asan.out nanoRegex.a
	size -t nanoRegex.a

main.out: main.cpp nanoRegex.a nanoRegex.hpp
	g++ $(CFLAGS) -Os -DNDEBUG main.cpp nanoRegex.a -o main.out

nanoRegex.a: impl.cpp nanoRegex.hpp
	g++ $(CFLAGS) -Os -DNDEBUG -c impl.cpp -o impl.o
	ar rcu nanoRegex.a impl.o
	rm impl.o


test.out: main.cpp test.o nanoRegex.hpp
	g++ $(CFLAGS) -Og -g main.cpp test.o -o test.out # -Wl,-M

test.o: impl.cpp nanoRegex.hpp
	g++ $(CFLAGS) -Og -g -c impl.cpp -o test.o


asan.out: main.cpp asan.o nanoRegex.hpp
	g++ $(CFLAGS) -Og -g -fsanitize=address main.cpp test.o -o asan.out

asan.o: impl.cpp nanoRegex.hpp
	g++ $(CFLAGS) -Og -g -fsanitize=address -c  impl.cpp -o asan.o
