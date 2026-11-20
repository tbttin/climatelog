INSTALL = install
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
DBDIR = /var/lib/climatelog
SYSDDIR = /etc/systemd/system

CC = gcc
CPPFLAGS += -Iinclude -MMD -MP
CFLAGS ?= -O2
LDLIBS += -lm -lsqlite3

TARGET ?= climatelog
OBJDIR ?= obj
SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c, $(OBJDIR)/%.o, $(SRCS))
DEPS = $(OBJS:.o=.d)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	@mkdir -p $@

-include $(DEPS)

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
