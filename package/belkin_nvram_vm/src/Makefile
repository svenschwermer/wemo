# ============================================================================
# Name        : Makefile
# Author      : Abhishek.Agarwal
# Version     : 15 October' 2012
# Copyright   :
# Description : 
# ============================================================================

MAIN_FILENAME = nvram
LIB_FILENAME = libbelkin_nvram.so

LIB_CFLAGS  = $(CFLAGS) -shared -fPIC
LIB_LDFLAGS = $(LDFLAGS) -Wl,-soname,$(LIB_FILENAME)

MAIN_CFLAGS := -Wall $(CFLAGS)
MAIN_LDFLAGS = $(LDFLAGS)

MAIN_OBJ = fw_env_main.o
LIB_OBJ = fw_env.o

ifeq ($(MTD_VERSION),old)
MAIN_CFLAGS += -DMTD_OLD
endif

all: main libbelkin_nvram

main: libbelkin_nvram
	$(CC) $(MAIN_CFLAGS) -c -o fw_env_main.o fw_env_main.c
	$(CC) -o $(MAIN_FILENAME) $(MAIN_LDFLAGS) $(MAIN_OBJ) \
		$(LIB_FILENAME)
	ln -s nvram nvram_set
	ln -s nvram nvram_get

fw_env_main.o: fw_env_main.c
	$(CC) $(MAIN_CFLAGS) -c -o $@ $<

libbelkin_nvram:
	$(CC) $(LIB_CFLAGS) -c -o fw_env.o fw_env.c
	$(CC) $(LIB_CFLAGS) $(LIB_LDFLAGS) \
		-o $(LIB_FILENAME) $(LIB_OBJ)

clean:
	rm -f $(MAIN_FILENAME) $(LIB_FILENAME) *.o
