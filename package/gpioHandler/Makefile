include $(TOPDIR)/rules.mk

PKG_NAME:=gpioHandler
PKG_RELEASE:=1

# PKG_SOURCE:=$(PKG_NAME).tar.gz
# PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
# PKG_MD5SUM:=0cfd04f4ff9b0651f41d2ad371c46027

include $(INCLUDE_DIR)/package.mk

define Package/gpioHandler
	SECTION:=utils
	CATEGORY:=WNC utils
	DEFAULT:=n
	TITLE:=gpioHandler utility
	MENU:=1
endef

define Package/gpioHandler/description
	gpioHandler is a for handling GPIO event from button
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(call Build/Compile/Default)
	$(MAKE) -C $(PKG_BUILD_DIR)
endef

define Package/gpioHandler/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/gpioHandler $(1)/usr/sbin/gpioHandler
endef

$(eval $(call BuildPackage,gpioHandler))

