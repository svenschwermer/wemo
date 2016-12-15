CFLAGS+=-g -pthread -std=c99
CFLAGS+=-Wall -Werror
LDLIBS+=-lpthread

# Lifted from dmalloc Makefile
INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644
SRCDIR = .
BINDIR = /usr/bin

DMTEST_MT:=true

ifndef NO_DMALLOC
CFLAGS+=-DDMALLOC
CFLAGS+=-DDMALLOC_FUNC_CHECK
ifdef DMALLOC_ENV
CFLAGS+=-DDMALLOC_ENV
endif
ifdef DMTEST_MT
LDLIBS+=-ldmallocth
CFLAGS+=-DDMTEST_MT
else
LDLIBS+=-ldmalloc
endif
endif

DMTEST:=dmtest
THREADS:=threads
OBJS:=$(DMTEST).o $(THREADS).o

.PHONY: all clean install test

all: $(DMTEST)

$(DMTEST): $(OBJS)

$(OBJS).o : dmtest.h

test: $(DMTEST)
	./dmtest one two

install: $(DMTEST)
	$(info Note: DESTDIR is $(DESTDIR))
	$(info Note: bindir is $(BINDIR))
	mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL_PROGRAM) $(DMTEST) $(DESTDIR)$(BINDIR)

clean:
	-@rm $(DMTEST) *.o 2>/dev/null
