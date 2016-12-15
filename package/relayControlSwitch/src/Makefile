BINDIR        = $(PREFIX)/usr/sbin
INSTALL       = install
EXEC          = relayControlSwitch
SRCS          = relayControlSwitch.c

all: $(EXEC)

$(EXEC): $(SRCS)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) -o $@ $@.c

install:
	$(INSTALL) -d $(BINDIR)/$(EXEC)

clean:
	-rm -f $(EXEC) *.o

