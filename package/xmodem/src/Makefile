BINDIR        = $(PREFIX)/usr/sbin
INSTALL       = install
EXEC          = xmodem
SRCS          = xmodem.c
LDFLAGS      += -static
LDLIBS       += -lpthread

all: $(EXEC)

$(EXEC): $(SRCS)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -lpthread $(LDFLAGS) -o $@ $@.c $(LDLIBS)

install:
	$(INSTALL) -d $(BINDIR)/$(EXEC)

clean:
	-rm -f $(EXEC) *.o


