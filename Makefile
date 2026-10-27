CC = gcc
LDFLAGS += -Iinclude
LDLIBS += -lm -lsqlite3
TARGET = climatelog

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c, obj/%.o, $(SRCS))

$(TARGET): $(OBJS)
	$(CC) $(LDLIBS) $^ -o $@

obj/%.o: src/%.c | obj
	$(CC) $(LDFLAGS) -c -o $@ $<

obj:
	@mkdir $@

obj/climate_log.o: include/sensor.h include/db.h
obj/sensor.o: include/sensor.h include/config.h
obj/db.o: include/db.h include/sensor.h include/config.h

.PHONY: clean

clean::
	rm -rf obj/ $(TARGET)
