include $(TOPDIR)/rules.mk

PKG_NAME:=sysevent
PKG_RELEASE:=1
PKG_BUILD_DEPENDS:= ulog 

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

define SYSEVENT
all: sysevent syseventd_fork_helper
sysevent: LDFLAGS += -L. -lsysevent -pthread 
sysevent: $(OBJS_C)
	$$(CC) -o $$@ $$^ $$(LDFLAGS)
syseventd: LDFLAGS += -L. -lsysevent -lulog -pthread
syseventd: $(OBJS_D) 
	$$(CC) -o $$@ $$^ $$(LDFLAGS)
syseventd_fork_helper: LDFLAGS += -L. -lsysevent -lulog -pthread
syseventd_fork_helper: $(OBJS_F)
	$$(CC) -o $$@ $$^ $$(LDFLAGS)
endef

export WEMO_LIB
export SYSEVENT
export OUTPUT := libsysevent.so 
export OBJS   := $(patsubst %.c,lib/%.o, $(notdir $(wildcard src/lib/*.cpp) $(wildcard src/lib/*.c)))
export OBJS_C := $(patsubst %.c,control/%.o, $(notdir $(wildcard src/control/*.cpp) $(wildcard src/control/*.c)))
export OBJS_D := $(patsubst %.c,server/%.o, $(notdir $(wildcard src/server/*.cpp) $(wildcard src/server/*.c)))
export OBJS_F := $(patsubst %.c,fork_helper/%.o, $(notdir $(wildcard src/fork_helper/*.cpp) $(wildcard src/fork_helper/*.c)))
export VPATH  := $(CURDIR)/src 

EXTRA_CFLAGS += -Wall -g -fPIC

define Package/sysevent
  CATEGORY	:= Belkin
  SUBMENU	:= Libraries
  SECTION	:= libs
  TITLE		:= sysevent lib and daemon
  DEPENDS	+= +ulog
endef

define Package/sysevent/description
	sysevent lib and deamon
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$SYSEVENT" >> $(PKG_BUILD_DIR)/Makefile
	echo "$$$$WEMO_LIB" >> $(PKG_BUILD_DIR)/Makefile
	$(CP) src/*  $(PKG_BUILD_DIR)/.
endef

define Build/Compile
	$(MAKE_VARS) $(MAKE) -C $(PKG_BUILD_DIR)/$(MAKE_PATH) \
		$(MAKE_FLAGS) $(OUTPUT)
	$(CP) src/lib/*.h $(STAGING_DIR)/include
	$(MAKE_VARS) $(MAKE) -C $(PKG_BUILD_DIR)/$(MAKE_PATH) \
		$(MAKE_FLAGS) sysevent
	$(MAKE_VARS) $(MAKE) -C $(PKG_BUILD_DIR)/$(MAKE_PATH) \
		$(MAKE_FLAGS) syseventd
	$(MAKE_VARS) $(MAKE) -C $(PKG_BUILD_DIR)/$(MAKE_PATH) \
		$(MAKE_FLAGS) syseventd_fork_helper
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libsysevent.so* $(1)/usr/lib
endef

define Package/sysevent/install
	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libsysevent.so* $(1)/lib
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sysevent $(1)/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/syseventd $(1)/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/syseventd_fork_helper $(1)/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/syseventd.init $(1)/etc/init.d/syseventd
endef

$(eval $(call BuildPackage,sysevent))

