CFLAGS := -Wall -O -g
INC := -I./include

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c, obj/%.o, $(SRC))

CC     := gcc

TARGET := que_recv que_send

all: $(TARGET)

que_recv: que_recv.o que.o rbtree.o
	$(CC) -o $@ $^ -lpthread

que_send: que_send.o que.o rbtree.o
	$(CC) -o $@ $^ -lpthread

$(OBJ): obj/%.o : src/%.c
	$(CC) -c $(CFLAGS) -o $@ $< $(INC)

clean:
	-rm $(OBJ) $(TARGET)

.PHONY: all clean 

vpath %.c  src
vpath %.o  obj
vpath %.h  include
