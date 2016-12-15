include $(TOPDIR)/rules.mk

PKG_NAME:=gpio_test
PKG_RELEASE:=1

# PKG_SOURCE:=$(PKG_NAME).tar.gz
# PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
#PKG_MD5SUM:=106a5d7cff5b2e63fe6623844552d533

include $(INCLUDE_DIR)/package.mk

define Package/gpio_test
	SECTION:=utils
	CATEGORY:=WNC utils
	DEFAULT:=n
	TITLE:=gpio_test utility
	MENU:=1
endef

define Package/gpio_test/description
	gpio_test is an example for controlling PING 0~22 GPIO
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(call Build/Compile/Default)
	$(MAKE) -C $(PKG_BUILD_DIR)/
endef

define Package/gpio_test/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/gpio_test $(1)/usr/sbin/gpio_test
endef

$(eval $(call BuildPackage,gpio_test))

