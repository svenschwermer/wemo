OBJDIR = .

LIB_NAME = libWasp
LIB_FILENAME = $(LIB_NAME).so
LIBS = $(OBJDIR)/$(LIB_FILENAME)

LIB_SRCS =  wasp_api log
DAEMON_SRCS = waspd wasp_async log pseudo

LIB_CFLAGS  = $(CFLAGS) -shared -fPIC
LIB_LDFLAGS = $(LDFLAGS) -Wl,-soname,$(LIB_FILENAME)

CLI_LDFLAGS = $(LDFLAGS)

CFLAGS += -Wall

ifeq ($(DEBUG),y)
CFLAGS += -O0 -g -DDEBUG
CXXFLAGS += -O0 -g
LDFLAGS = -g
else
CFLAGS += -Os -fomit-frame-pointer
endif

ifneq ($(GEMTEK_SDK),)
CFLAGS += -DGEMTEK_SDK=$(GEMTEK_SDK)
LIBS += $(SDKPATH)/user/nvram/nvram/libnvram.so
LIBS += $(SDKPATH)/user/gemtek_api/gemtek_api/libgemtek_api.so
endif

ifeq ($(SIM_CROCKPOT),y)
DAEMON_SRCS += crockpot_cmd jarden
CFLAGS += -DSIMULATE_DEVICE -funsigned-char -DSIMULATE_JARDEN
endif

ifeq ($(SIM_MRCOFFEE),y)
DAEMON_SRCS += mrcoffee_cmd
CFLAGS += -DSIMULATE_DEVICE -funsigned-char
endif

ifeq ($(CROCKPOT_HW),y)
CFLAGS += -DCROCKPOT_HW
LIBS += ../../crockpot/src/libCrockPot.so
endif

ifeq ($(OLD_API),y)
CFLAGS += -DOLD_API
LIB_SRCS += old_api
endif

ifeq ($(GEMTEK_SDK),y)
CFLAGS += -DGEMTEK_SDK
endif

SRCS = $(LIB_SRCS) $(DAEMON_SRCS)
LIB_OBJS = $(patsubst %,$(OBJDIR)/%.o,$(LIB_SRCS))
DAEMON_OBJS = $(patsubst %,$(OBJDIR)/%.o,$(DAEMON_SRCS))

.PHONY: all clean


#all: waspd $(LIB_FILENAME)

$(OBJDIR)/%.o: %.c
	$(CC) $(LIB_CFLAGS) -c -o $@ $<

$(OBJDIR)/waspd: $(DAEMON_OBJS) $(LIBS)
	echo "Making waspd with CFLAGS $(CFLAGS)"
	$(CC) -o $@ $(CLI_LDFLAGS) $^

$(OBJDIR)/$(LIB_FILENAME): $(LIB_OBJS)
	$(CC) $(LIB_CFLAGS) $(LIB_LDFLAGS) -o $(OBJDIR)/$(LIB_FILENAME) $^

$(OBJDIR)/old_api.o: old_api.c
	$(CC) $(LIB_CFLAGS) -c -o $@ $<

$(OBJDIR)/wasp_api.o: wasp_api.c
	$(CC) $(LIB_CFLAGS) -c -o $@ $<

$(OBJDIR)/log.o: log.c
	$(CC) $(LIB_CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir $(OBJDIR)

clean:
ifneq ($(OBJDIR),.)
	-@rm -rf $(OBJDIR) > /dev/null
else
	rm -f waspd $(LIB_FILENAME)* *.o .depend
endif

$(OBJDIR)/.depend: $(OBJDIR) Makefile $(patsubst %,%.c,$(SRCS))
	$(CC) -M $(CFLAGS) $(patsubst %,%.c,$(SRCS)) > $@

ifneq ($(MAKECMDGOALS),clean)
-include $(OBJDIR)/.depend
endif

