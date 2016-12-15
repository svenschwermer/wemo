include $(TOPDIR)/rules.mk

PKG_NAME:=Handlers
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export WEMO_LIB
export OUTPUT := libPluginHndlr.so
export OBJS := $(patsubst %.c,%.o,$(notdir $(wildcard src/*.cpp) $(wildcard src/*.c)))
export VPATH := $(CURDIR)/src

EXTRA_CFLAGS += -DDECRYPT_PASSWORD
EXTRA_CFLAGS += -fpic -I$(STAGING_DIR)/usr/include/upnp
EXTRA_CFLAGS += -D__WIRED_ETH__

define Package/Handlers
  CATEGORY := Belkin
  SUBMENU  := Libraries
  SECTION  := libs
  TITLE    := Handlers
  DEPENDS  := +WeMo_Core +libmxml +AddlInfra +libopenssl +RemoteAccess
endef

define Package/Handlers/description
	WeMo application event handler library
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_LIB" > $(PKG_BUILD_DIR)/Makefile
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/usr/lib
endef

define Package/Handlers/install
	$(INSTALL_DIR) $(1)/lib
	$(CP) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/lib/
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) ./files/chkping.sh $(1)/sbin/chkping.sh
endef

$(eval $(call BuildPackage,Handlers))
