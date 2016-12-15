LIB_NAME = libCrockPot
LIB_FILENAME = $(LIB_NAME).so

LIB_CFLAGS  = $(CFLAGS) -shared -fPIC
LIB_LDFLAGS = $(LDFLAGS) -Wl,-soname,$(LIB_FILENAME)

#override CFLAGS += -DSIMULATE

CLI_LDFLAGS = $(LDFLAGS)

all: crockpotd $(LIB_FILENAME)

crockpotd: crockpotd.c crockpot.h crockpotd.h $(LIB_FILENAME)
	echo "Making Crockpotd with CFLAGS $(CFLAGS)"
	$(CC) $(CFLAGS) -c -o crockpotd.o crockpotd.c
	$(CC) -o $@ $(CLI_LDFLAGS) $@.o $(LIB_FILENAME)

$(LIB_FILENAME): crockpot.c crockpot.h crockpotd.h
	$(CC) $(LIB_CFLAGS) -c -o $(LIB_NAME).o crockpot.c
	$(CC) $(LIB_CFLAGS) $(LIB_LDFLAGS) -o $(LIB_FILENAME) $(LIB_NAME).o

clean:
	rm -f crockpotd $(LIB_FILENAME)* *.o
