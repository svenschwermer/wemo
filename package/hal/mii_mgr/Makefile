include $(TOPDIR)/rules.mk

PKG_NAME:=mii_mgr
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

define Package/mii_mgr
  CATEGORY:=Belkin
  SUBMENU  := Utilities
  SECTION  := utils
  TITLE:=Ralink MII manager
endef

define Package/mii_mgr/description
 This tool is used to access MMI registers on the Ralink SOC
endef

TARGET_CFLAGS := 
define SUBMAKE_MK

all: mii_mgr
mii_mgr: mii_mgr.o

endef
export SUBMAKE_MK
export VPATH  := $(CURDIR)/src

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$SUBMAKE_MK" > $(PKG_BUILD_DIR)/Makefile
endef

define Package/mii_mgr/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mii_mgr $(1)/sbin/
endef

$(eval $(call BuildPackage,mii_mgr))

