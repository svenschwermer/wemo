# NVDIR = $(ROOTDIR)/lib/libnvram
# CFLAGS += -I$(NVDIR) 
# LDFLAGS += -lnvram
# EXEC = ated
# SRCS = ated.c
# 
# #EXTRA_CFLAGS += -DDBG
# 
# all: $(EXEC)
# 
# $(EXEC): $(SRCS) $(ROOTDIR)/$(LINUXDIR)/include/linux/autoconf.h
# 	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) -o $@ $@.c $(LDLIBS)
# 
# romfs:
# 	$(ROMFSINST) /bin/$(EXEC)
# 
# clean:
# 	-rm -f $(EXEC) *.o
# 

include $(TOPDIR)/rules.mk

PKG_NAME:=wemotest
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/wemotest
  CATEGORY:=Belkin
  SUBMENU:=Deamons
  SECTION:=utils
  TITLE:=Belkin factory test deamon
endef

define Package/wemotest/description
	This tool is used to do factory test  
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) -Wall" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/wemotest/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wemotest $(1)/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/wemotest.init $(1)/etc/init.d/wemotest
endef

$(eval $(call BuildPackage,wemotest))

