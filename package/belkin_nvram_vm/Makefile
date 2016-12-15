# ============================================================================
# Name        : Makefile
# Author      : Abhishek.Agarwal
# Version     : 15 October' 2012
# Copyright   :
# Description : 
# ============================================================================

include $(TOPDIR)/rules.mk

PKG_NAME:=belkin_nvram_vm
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

LIBNAME := lib$(PKG_NAME).so

include $(INCLUDE_DIR)/package.mk

define Package/belkin_nvram_vm
  CATEGORY:=Belkin
  SUBMENU:=Libraries
  SECTION:=libs
  TITLE:=Belkin Virtual Machine NVRAM Library
endef

define Package/belkin_nvram_vm/description
	Library to write to NVRAM flash partition
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) -Wall" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libbelkin_nvram.so $(1)/lib/
endef

define Package/belkin_nvram_vm/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/nvram $(1)/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/nvram_get $(1)/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/nvram_set $(1)/sbin/
	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libbelkin_nvram.so $(1)/lib/
endef

$(eval $(call BuildPackage,belkin_nvram_vm))
