CFLAGS=-Wall -Wextra -Werror -pedantic -Os -s
all: nanoRegex.a a.out
	size -t nanoRegex.a

a.out: main.cpp nanoRegex.a nanoRegex.hpp
	g++ $(CFLAGS) main.cpp nanoRegex.a -o a.out

nanoRegex.a: impl_parse.o impl_match.o
	ar rcu nanoRegex.a impl_parse.o impl_match.o

impl_parse.o: impl_parse.cpp nanoRegex.hpp
	g++ -c $(CFLAGS) impl_parse.cpp -o impl_parse.o

impl_match.o: impl_match.cpp nanoRegex.hpp
	g++ -c $(CFLAGS) impl_match.cpp -o impl_match.o

clean:
	rm -f *.o a.out
