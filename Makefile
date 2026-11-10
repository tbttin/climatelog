INSTALL = install
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
DBDIR = /var/lib/climatelog
SYSDDIR = /etc/systemd/system

CC = gcc
CPPFLAGS += -Iinclude
CFLAGS ?= -O2
LDLIBS += -lm -lsqlite3

TARGET ?= climatelog
OBJDIR ?= obj
SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c, $(OBJDIR)/%.o, $(SRCS))

$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDLIBS)

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	@mkdir -p $@

$(OBJDIR)/climate_log.o: include/sensor.h include/db.h
$(OBJDIR)/sensor.o: include/sensor.h include/config.h
$(OBJDIR)/db.o: include/db.h include/sensor.h include/config.h

.PHONY: clean install uninstall

install: $(TARGET) climatelog.service
	id -u climatelog >/dev/null 2>&1 || useradd --system --no-create-home --shell /usr/sbin/nologin climatelog
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	$(INSTALL) -m 644 -o root -g root climatelog.service $(SYSDDIR)/climatelog.service
	$(INSTALL) -d -m 750 -o climatelog -g climatelog $(DBDIR)
	@echo 'Done.'

uninstall:
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(SYSDDIR)/climatelog.service
	@echo "Uninstallation complete. Note: $(DBDIR)/climatelog.db was kept safe to preserve your history."

clean::
	rm -rf $(OBJDIR) $(TARGET)
