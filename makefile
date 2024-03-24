app: lib init user1.c user2.c
	gcc -Wall -I. user1.c -o user1 -L. -lmsocket
	gcc -Wall -I. user2.c -o user2 -L. -lmsocket

lib: msocket.h msocket.o
	ar rcs libmsocket.a msocket.o

msocket.o: msocket.c msocket.h
	gcc -Wall -c -I. msocket.c

init: initmsocket.c
	gcc -Wall -I. -DVERBOSE initmsocket.c -o initmsocket -L. -lmsocket

run: app
	./initmsocket

clean:
	rm -f *.o *.a *.out user1 user2 initmsocket