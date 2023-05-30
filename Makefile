CFLAGS=-Wall -Wextra -Werror -pedantic -O2

a.out: main.cpp nanoRegex.o nanoRegex.hpp
	g++ $(CFLAGS) nanoRegex.o main.cpp -o a.out
CFILES=impl_parse.cpp impl_match.cpp
nanoRegex.o: $(CFILES) nanoRegex.hpp impl_parse.hpp impl_match.hpp
	g++ -r $(CFLAGS) $(CFILES) -o nanoRegex.o

clean:
	rm -f *.o a.out
