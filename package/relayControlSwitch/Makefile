include $(TOPDIR)/rules.mk

PKG_NAME:=relayControlSwitch
PKG_RELEASE:=1

# PKG_SOURCE:=$(PKG_NAME).tar.gz
# PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
#PKG_MD5SUM:=0cfd04f4ff9b0651f41d2ad371c46027

include $(INCLUDE_DIR)/package.mk

define Package/relayControlSwitch
	SECTION:=utils
	CATEGORY:=WNC utils
	DEFAULT:=n
	TITLE:=relayControlSwitch utility
	MENU:=1
endef

define Package/relayControlSwitch/description
	relayControlSwitch is a for handling GPIO event from power meter
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(call Build/Compile/Default)
	$(MAKE) -C $(PKG_BUILD_DIR)/
endef

define Package/relayControlSwitch/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/relayControlSwitch $(1)/usr/sbin/relayControlSwitch
endef

$(eval $(call BuildPackage,relayControlSwitch))

