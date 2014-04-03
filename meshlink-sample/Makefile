# Simple Makefile

CC=gcc
LIBS= -lcrypto  -lz -llzo2 -ldl -lpthread -L../src/.libs/ -lmeshlink
INCLUDE= -I../src/
LOADLIBES= $(LIBS)
OBJS= meshlinkapp.o
CFLAGS= -g -D_PC_ $(INCLUDE) -Wall
EXEC=meshlinkapp
default: all
all: $(OBJS) $(EXEC)
$(EXEC): $(OBJS)
clean:
	-rm -f $(OBJS)
	-rm -f $(EXEC)


rebuild: clean all
