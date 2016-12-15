EXE = 
OBJS = 
ROMFSDIR = ../../romfs
CFLAGS += -O0v

all: 

.cpp.o:

romfs:
	cp -rf lib $(ROMFSDIR)/
	cp -rf sbin $(ROMFSDIR)/
	cp -rf usr/lib/* $(ROMFSDIR)/lib/
	cp -rf etc/certs $(ROMFSDIR)/sbin/cacerts
clean:
