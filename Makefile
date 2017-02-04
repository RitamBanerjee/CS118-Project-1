CC=gcc
CFLAGS=-I.
DEPS = # header file
OBJ = webserver.o

%.o: %.c $(DEPS)
	$(CC) -c -o  $@ $< $(CFLAGS)

webserver: $(OBJ)
	$(CC) -o -g  $@ $^ $(CFLAGS)

clean: 
	\rm -f *.o webserver