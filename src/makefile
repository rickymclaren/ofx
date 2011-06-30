CC     = gcc
CFLAGS = `pkg-config --cflags gtk+-2.0 libxml-2.0`
LIBS   = `pkg-config --libs gtk+-2.0 libxml-2.0`

ofx : main.o
	$(CC) $(LIBS) -o ofx main.o

main.o : main.c
	$(CC) $(CFLAGS) -c main.c

hello : hello.o
	$(CC) $(LIBS) -o hello hello.o

hello.o : hello.c
	$(CC) $(CFLAGS) -c hello.c


