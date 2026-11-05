INSTALL = install
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
DBDIR = /var/lib/climatelog
SYSDDIR = /etc/systemd/system

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

.PHONY: clean install uninstall

install: $(TARGET) climatelog.service
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	$(INSTALL) -m -o root -g root climatelog.service $(SYSDDIR)/climatelog.service
	$(INSTALL) -d $(DBDIR)
	@echo 'Done.'

uninstall:
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(SYSDDIR)/climatelog.service
	@echo "Uninstallation complete. Note: $(DBDIR)/climatelog.db was kept safe to preserve your history."

clean::
	rm -rf obj/ $(TARGET)
