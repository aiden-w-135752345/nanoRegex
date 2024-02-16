CFLAGS = -Wall -Wextra -Werror -Wpedantic -std=c++17
all: main.out test.out asan.out nanoRegex.a
	size -t main.out test.out asan.out nanoRegex.a

main.out: main.cpp nanoRegex.a nanoRegex.hpp cx.hpp
	g++ $(CFLAGS) -Os -DNDEBUG main.cpp nanoRegex.a -o $@

nanoRegex.a: impl.cpp nanoRegex.hpp cx.hpp
	g++ $(CFLAGS) -Os -DNDEBUG -c impl.cpp -o impl.o
	ar rcu $@ impl.o
	rm impl.o


test.out: main.cpp test.o nanoRegex.hpp cx.hpp
	g++ $(CFLAGS) -Og -g main.cpp test.o -o $@ # -Wl,-M

test.o: impl.cpp nanoRegex.hpp cx.hpp
	g++ $(CFLAGS) -Og -g -c impl.cpp -o $@


asan.out: main.cpp asan.o nanoRegex.hpp cx.hpp
	g++ $(CFLAGS) -Og -g -fsanitize=address main.cpp test.o -o $@

asan.o: impl.cpp nanoRegex.hpp cx.hpp
	g++ $(CFLAGS) -Og -g -fsanitize=address -c impl.cpp -o $@
