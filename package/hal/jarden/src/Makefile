LIB_NAME = libJarden
LIB_FILENAME = $(LIB_NAME).so

LIB_CFLAGS  = $(CFLAGS) -shared -fPIC
LIB_LDFLAGS = $(LDFLAGS) -Wl,-soname,$(LIB_FILENAME)

#override CFLAGS += -DSIMULATE

CLI_LDFLAGS = $(LDFLAGS)

all: jardend $(LIB_FILENAME)

jardend: jardend.c jarden.h jardend.h $(LIB_FILENAME)
	echo "Making Jardend with CFLAGS $(CFLAGS)"
	$(CC) $(CFLAGS) -c -o jardend.o jardend.c
	$(CC) -o $@ $(CLI_LDFLAGS) $@.o $(LIB_FILENAME)

$(LIB_FILENAME): jarden.c jarden.h jardend.h
	$(CC) $(LIB_CFLAGS) -c -o $(LIB_NAME).o jarden.c
	$(CC) $(LIB_CFLAGS) $(LIB_LDFLAGS) -o $(LIB_FILENAME) $(LIB_NAME).o

clean:
	rm -f jardend $(LIB_FILENAME)* *.o
