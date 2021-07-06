all:	cfg_parse.c main.c cfg_parse.h
	gcc -Wall -Wextra -ansi -O2 -pipe -fomit-frame-pointer -march=native -g -c cfg_parse.c
	gcc -Wall                         -O2 -pipe -fomit-frame-pointer -march=native -g -c main.c
	gcc -Wall -Wextra -ansi -pedantic -O2 -pipe -fomit-frame-pointer -march=native -g -o tmee *.o -ldl

clean:
	rm -f test *.o
