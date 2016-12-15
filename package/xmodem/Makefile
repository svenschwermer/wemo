include $(TOPDIR)/rules.mk

PKG_NAME:=xmodem
PKG_RELEASE:=1

#PKG_SOURCE:=$(PKG_NAME).tar.gz
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
#PKG_MD5SUM:=5921c765c856582ee1b6d446513d76b5

include $(INCLUDE_DIR)/package.mk

define Package/xmodem
	SECTION:=utils
	CATEGORY:=WNC utils
	DEFAULT:=n
	TITLE:=Xmodem Transfer Tool
	MENU:=1
endef

define Package/xmodem/description
	Daemon for Xmodem.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(call Build/Compile/Default)
	$(MAKE) -C $(PKG_BUILD_DIR)/
endef

define Package/xmodem/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/xmodem $(1)/usr/sbin/xmodem
endef

$(eval $(call BuildPackage,xmodem))

