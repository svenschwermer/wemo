include $(TOPDIR)/rules.mk

PKG_NAME := UPnP
PKG_RELEASE := 1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export WEMO_LIB
export OUTPUT := libUPnPHndlr.so
export OBJS := $(patsubst %.c,%.o,$(notdir $(wildcard src/*.cpp) $(wildcard src/*.c)))
export VPATH := $(CURDIR)/src

TARGET_CFLAGS += -fpic -I$(STAGING_DIR)/usr/include/upnp

define Package/UPnP
  CATEGORY := Belkin
  SUBMENU  := Libraries
  SECTION  := libs
  TITLE    := UPnP Libraries
  DEPENDS  := +libmxml +DeviceControl +Handlers # +RemoteAccess
endef

define Package/UPnP/description
	WeMo application UPnP interface
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_LIB" > $(PKG_BUILD_DIR)/Makefile
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/usr/lib
	$(CP) src/*.h $(STAGING_DIR)/include
endef

define Package/UPnP/install
	$(INSTALL_DIR) $(1)/lib
	$(CP) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/lib/
	$(INSTALL_DIR) $(1)/sbin
	$(CP) -rfa src/XMLs/web $(1)/sbin
	rm -rf $(1)/sbin/web/upnp
endef

$(eval $(call BuildPackage,UPnP))
