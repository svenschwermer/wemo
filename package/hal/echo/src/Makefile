LIB_NAME = libWasp
LIB_FILENAME = $(LIB_NAME).so
LIBS = $(LIB_FILENAME)

COMMON_SRCS = decode
DAEMON_SRCS = echod 
DECODER_SRCS = echo_decode 

CLI_LDFLAGS = $(LDFLAGS)

CFLAGS += -Wall -I../echo_frontend -I ../../wasp/src

ifeq ($(DEBUG),y)
CFLAGS += -O0 -g -DDEBUG
CXXFLAGS += -O0 -g
LDFLAGS = -g
else
CFLAGS += -Os -fomit-frame-pointer
endif

SRCS = $(DAEMON_SRCS) $(DECODER_SRCS) $(COMMON_SRCS)
DAEMON_OBJS = $(patsubst %,%.o,$(DAEMON_SRCS) $(COMMON_SRCS))
DECODER_OBJS = $(patsubst %,%.o,$(DECODER_SRCS) $(COMMON_SRCS))

.PHONY: all clean


echod: $(DAEMON_OBJS) $(LIBS)
	$(CC) -o $@ $(CLI_LDFLAGS) $^

echo_decode: $(DECODER_OBJS)
	$(CC) -o $@ $(CLI_LDFLAGS) $^

clean:
	rm -f echod echo_decode *.o .depend

.depend: Makefile $(patsubst %,%.c,$(SRCS))
	$(CC) -M $(CFLAGS) $(patsubst %,%.c,$(SRCS)) > $@

ifneq ($(MAKECMDGOALS),clean)
-include .depend
endif

