CC = gcc
LDFLAGS += -Iinclude
LDLIBS += -lm
TARGET = climatelog

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c, obj/%.o, $(SRCS))

$(TARGET): $(OBJS)
	$(CC) $(LDLIBS) $^ -o $@

obj/%.o: src/%.c | obj
	$(CC) $(LDFLAGS) -c -o $@ $<

obj:
	mkdir $@

obj/sensor.o: include/sensor.h include/config.h
obj/climate_log.o: include/sensor.h

.PHONY: clean

clean::
	rm -rf obj/ $(TARGET)
