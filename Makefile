CC := /usr/bin/gcc

all: clean | task1	

task1:
	$(CC) -o game.o src/game.c

clean: 
	rm -rf *.o
