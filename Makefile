robots:		robots.o highscores.o
		cc -O -o robots robots.o highscores.o -lcurses -lm

robots2:	robots\ v2.0.c
		cc -O robots\ v2.0.c -o robots -lcurses -lm

robots.o:	robots.c highscores.h
		cc -O -c robots.c

highscores.o:	highscores.c highscores.h
		cc -O -c highscores.c

install:	robots
		cp robots ~/Apps
		cp robots ~/Public

clean:
		rm -f *.o

