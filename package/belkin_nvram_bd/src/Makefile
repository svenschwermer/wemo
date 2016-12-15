OBJDIR = .

LIB_NAME = libbelkin_nvram
LIB_FILENAME = $(LIB_NAME).so
LIBS = -L$(OBJDIR) -lbelkin_nvram -lrt -lz -lpthread

APP_NAME = nvram

LIB_SRCS =  libnvram
DAEMON_SRCS = nvram fw_factory

LIB_CFLAGS  = $(CFLAGS) -shared -fPIC 
LIB_LDFLAGS = $(LDFLAGS) -Wl,-soname,$(LIB_FILENAME) -lz -lrt

CLI_LDFLAGS = $(LDFLAGS)

CFLAGS += -Wall -pthread

ifeq ($(DEBUG),y)
CFLAGS += -O0 -g -DDEBUG
CXXFLAGS += -O0 -g
LDFLAGS = -g
else
CFLAGS += -Os -fomit-frame-pointer
endif


SRCS = $(LIB_SRCS) $(DAEMON_SRCS)
LIB_OBJS = $(patsubst %,$(OBJDIR)/%.o,$(LIB_SRCS))
DAEMON_OBJS = $(patsubst %,$(OBJDIR)/%.o,$(DAEMON_SRCS))

.PHONY: clean


$(OBJDIR)/%.o: %.c
	$(CC) $(LIB_CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(OBJDIR)/$(APP_NAME): $(DAEMON_OBJS) $(OBJDIR)/$(LIB_FILENAME)
	$(CC) -o $@ $(CLI_LDFLAGS) $^ $(LIBS)
	ln -sf $(OBJDIR)/$(APP_NAME) $(OBJDIR)/$(APP_NAME)d
	ln -sf $(OBJDIR)/$(APP_NAME) $(OBJDIR)/$(APP_NAME)_get
	ln -sf $(OBJDIR)/$(APP_NAME) $(OBJDIR)/$(APP_NAME)_set

$(OBJDIR)/$(LIB_FILENAME): $(LIB_OBJS)
	$(CC) $(LIB_CFLAGS) -o $(OBJDIR)/$(LIB_FILENAME) $^ $(LIB_LDFLAGS) 

$(OBJDIR):
	mkdir $(OBJDIR)

clean:
ifneq ($(OBJDIR),.)
	-@rm -rf $(OBJDIR) > /dev/null
else
	rm -f $(OBJDIR)/$(APP_NAME)
	rm -f $(OBJDIR)/$(APP_NAME)d
	rm -f $(OBJDIR)/$(APP_NAME)_get
	rm -f $(OBJDIR)/$(APP_NAME)_set
	rm -f  $(OBJDIR)/$(LIB_FILENAME)* $(OBJDIR)/*.o $(OBJDIR)/.depend
endif

$(OBJDIR)/.depend: $(OBJDIR) Makefile $(patsubst %,%.c,$(SRCS))
	$(CC) -M $(CFLAGS) $(CPPFLAGS) $(patsubst %,%.c,$(SRCS)) > $@

ifneq ($(MAKECMDGOALS),clean)
-include $(OBJDIR)/.depend
endif

