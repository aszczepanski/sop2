CC=gcc
LD=gcc

CFLAGS=-Wall
LDFLAGS=-Wall

SRCS=main.c client.c server.c
HDRS=main.h common.h client.h server.h
TARGET=prog

OBJS=$(addprefix ./obj/, $(addsuffix .o, $(SRCS)))
HOBJS=$(addprefix ./src/, $(HDRS))

$(TARGET): $(OBJS) $(HOBJS)
	$(LD) -o $(TARGET) $(OBJS) $(LDFLAGS)
./obj/%.c.o: ./src/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm $(OBJS) $(TARGET) 
