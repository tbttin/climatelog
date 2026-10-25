CC = gcc
TARGET = climatelog

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c, obj/%.o, $(SRCS))

$(TARGET): $(OBJS)
	$(CC) $^ -o $@

obj/%.o: src/%.c | obj
	$(CC) -c -o $@ $<

obj:
	mkdir $@

.PHONY: clean

clean:
	rm -rf obj/ $(TARGET)
