CC     = gcc
CFLAGS = `pkg-config --cflags gtk+-2.0 libxml-2.0`
LIBS   = `pkg-config --libs gtk+-2.0 libxml-2.0`

ofx : ofx.o
	$(CC) $(LIBS) -o ofx ofx.o

ofx.o : ofx.c
	$(CC) $(CFLAGS) -c ofx.c

hello : hello.o
	$(CC) $(LIBS) -o hello hello.o

hello.o : hello.c
	$(CC) $(CFLAGS) -c hello.c


