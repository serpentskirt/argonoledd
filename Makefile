CC = gcc
CFLAGS = -Wall -Wextra -Wshadow -Wformat=2 -O2
ASAN_CFLAGS = -g -O1 -fsanitize=address -fno-omit-frame-pointer
SRCS = main.c command.c oled.c render.c font.c stats.c util.c screen.c log.c app.c config.c socket.c

PREFIX    ?= /usr/local
BINDIR     = $(PREFIX)/sbin
DATADIR    = $(PREFIX)/share/argonoledd
SERVICEDIR = /etc/systemd/system
CONFFILE   = /etc/argonoledd.conf

all: argonoledd

argonoledd: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

.PHONY: asan
asan: CFLAGS := $(CFLAGS) $(ASAN_CFLAGS)
asan: clean argonoledd
	@echo "Built with ASAN (run './argonoledd --foreground' to check)"

.PHONY: install
install: argonoledd
	@echo "Installing binary..."
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 argonoledd $(DESTDIR)$(BINDIR)/argonoledd
	@echo "Installing assets..."
	install -d $(DESTDIR)$(DATADIR)/res
	install -m 0644 res/*.bin $(DESTDIR)$(DATADIR)/res/
	@echo "Installing service..."
	install -d $(DESTDIR)$(SERVICEDIR)
	install -m 0644 packaging/argonoledd.service $(DESTDIR)$(SERVICEDIR)/argonoledd.service
	@echo "Installing default config (skipped if already present)..."
	[ -f $(DESTDIR)$(CONFFILE) ] || install -m 0644 packaging/argonoledd.conf $(DESTDIR)$(CONFFILE)
	@echo "Creating argonoledd group..."
	getent group argonoledd >/dev/null 2>&1 || groupadd --system argonoledd
	systemctl daemon-reload
	systemctl enable argonoledd
	systemctl restart argonoledd
	@echo "Install complete. Use 'journalctl -u argonoledd -f' to follow logs."
	@echo ""
	@echo "  To allow a user to send socket commands:"
	@echo "    sudo usermod -aG argonoledd <user>"
	@echo "  (for group membership to take effect log out and back in, or run: newgrp argonoledd)"

.PHONY: uninstall
uninstall:
	-systemctl stop argonoledd
	-systemctl disable argonoledd
	rm -f $(DESTDIR)$(BINDIR)/argonoledd
	rm -rf $(DESTDIR)$(DATADIR)
	rm -f $(DESTDIR)$(SERVICEDIR)/argonoledd.service
	systemctl daemon-reload
	@echo "Uninstall complete. Config $(CONFFILE) left in place."
	@echo ""
	@echo "  Group 'argonoledd' not removed automatically — check members first:"
	@echo "    getent group argonoledd"
	@echo "    sudo groupdel argonoledd"

.PHONY: update
update: argonoledd
	@echo "Updating binary and assets..."
	install -m 0755 argonoledd $(DESTDIR)$(BINDIR)/argonoledd
	install -d $(DESTDIR)$(DATADIR)/res
	install -m 0644 res/*.bin $(DESTDIR)$(DATADIR)/res/
	install -m 0644 packaging/argonoledd.service $(DESTDIR)$(SERVICEDIR)/argonoledd.service
	@echo "Ensuring argonoledd group exists..."
	getent group argonoledd >/dev/null 2>&1 || groupadd --system argonoledd
	systemctl daemon-reload
	systemctl restart argonoledd
	@echo "Update complete."

clean:
	rm -f argonoledd *.o
