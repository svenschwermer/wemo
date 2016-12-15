include $(TOPDIR)/rules.mk

PKG_NAME:=dmtest
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

SRCDIR := $(CURDIR)/src
export VPATH := $(SRCDIR)

define Package/dmtest
  CATEGORY := Belkin
  SUBMENU  := Utilities
  SECTION  := utils
  TITLE    :=dmtest
  DEPENDS  :=+libdmalloc
endef

define Package/dmtest/description
Simple test for dmalloc package.
endef

TARGET_CFLAGS += $(FPIC)

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) $(SRCDIR)/Makefile $(PKG_BUILD_DIR)
endef

define Package/dmtest/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dmtest $(1)/usr/bin/
endef

$(eval $(call BuildPackage,dmtest))
