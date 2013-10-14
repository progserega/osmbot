osmpatch: osmpatch.o
	gcc osmpatch.o -o osmpatch -lxml2

osmpatch.o: osmpatch.c
	gcc -g3 -DDEBUG -c -I/usr/include/libxml2 osmpatch.c -o osmpatch.o

clean:
	rm osmpatch osmpatch.o
