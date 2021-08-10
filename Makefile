#all:	cfg_parse.c main.c cfg_parse.h
#	gcc -Wall -Wextra -ansi -O2 -pipe -fomit-frame-pointer -march=native -g -c cfg_parse.c
#	gcc -Wall                         -O2 -pipe -fomit-frame-pointer -march=native -g -c main.c
#	gcc -Wall -Wextra -ansi -pedantic -O2 -pipe -fomit-frame-pointer -march=native -g -o tmee *.o -ldl

all:	cfg_parse.c main.c cfg_parse.h
	g++ -Wall -Wextra -ansi -O2 -pipe -fomit-frame-pointer -fpermissive -march=native -g -c cfg_parse.c
	g++ -Wall                         -O2 -pipe -fomit-frame-pointer -fpermissive -march=native -g -I xpedite/include -c main.c
	g++ -Wall -Wextra -ansi -pedantic -O2 -pipe -fomit-frame-pointer -fpermissive -march=native -g -o tmee *.o -L xpedite/lib -lxpedite-pie -ldl -lrt -lpthread



clean:
	rm -f test *.o
