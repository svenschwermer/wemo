include $(TOPDIR)/rules.mk

PKG_NAME := NetworkControl
PKG_RELEASE := 1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export WEMO_LIB
export OUTPUT := libNetworkControl.so
export OBJS := $(patsubst %.c,%.o,$(notdir $(wildcard src/*.cpp) $(wildcard src/*.c)))
export VPATH := $(CURDIR)/src

EXTRA_CFLAGS += -DDECRYPT_PASSWORD
EXTRA_CFLAGS += -fpic -I$(STAGING_DIR)/usr/include/upnp
EXTRA_LDFLAGS +=-lbelkin_api

define Package/NetworkControl
  CATEGORY := Belkin
  SUBMENU  := Libraries
  SECTION  := libs
  TITLE    := NetworkControl
#  DEPENDS  := +UPnP +HalInsight
  DEPENDS  := +WeMo_Core +RemoteAccess +libupnp +belkin_api +sqlite
endef

define Package/NetworkControl/description
	WeMo application network control library.
endef

define Build/Prepare
	$(CP) src/wifiSetup.h $(STAGING_DIR)/include
	$(CP) ../Handlers/src/wifiHndlr.h $(STAGING_DIR)/include
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_LIB" > $(PKG_BUILD_DIR)/Makefile
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/usr/lib
	$(INSTALL_DIR) $(1)/usr/include/belkin
	$(INSTALL_BIN) src/wifiSetup.h $(1)/usr/include/belkin
endef

define Package/NetworkControl/install
	$(INSTALL_DIR) $(1)/lib
	$(CP) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/lib/
endef

$(eval $(call BuildPackage,NetworkControl))
