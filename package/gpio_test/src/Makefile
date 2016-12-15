BINDIR        = $(PREFIX)/usr/sbin
INSTALL       = install
EXEC          = gpio_test
SRCS          = gpio_test.c
#CC            = /opt/buildroot-gcc342/bin/mipsel-linux-gcc

all: $(EXEC)

$(EXEC): $(SRCS)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) -o $@ $@.c

install:
	$(INSTALL) -d $(BINDIR)/$(EXEC)

clean:
	-rm -f $(EXEC) *.o

