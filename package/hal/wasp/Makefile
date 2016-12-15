include $(TOPDIR)/rules.mk

PKG_NAME:=wasp
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
#include $(INCLUDE_DIR)/belkin_diag.mk

define Package/wasp
  CATEGORY:=Belkin
  SUBMENU:=HAL
  SECTION:=libs
  TITLE:=wasp HAL
  #$(call BELKIN_DIAG_DEPENDS)
endef

define Package/wasp/description
 Hardware abstraction layer for WeMo smart module based products.
 Consists of shared library (libWasp.so.*) and a
 Daemon (waspd) that handles communications with 
 the product incorporating the WeMo smart module
endef

define Build/Prepare
endef

define Build/Configure
endef

define Build/Compile
	$(MAKE) -C src OBJDIR=$(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) -Wall" \
		CPPFLAGS="-I$(STAGING_DIR)/usr/include -I$(STAGING_DIR)/include" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/*.so* $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/include
	$(INSTALL_BIN) src/wasp.h $(1)/include
	$(INSTALL_BIN) src/wasp_api.h $(1)/include
endef

define Package/wasp/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/waspd.init $(1)/etc/init.d/waspd
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/waspd $(1)/sbin/
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libWasp.so* $(1)/usr/lib
endef

$(eval $(call BuildPackage,wasp))
