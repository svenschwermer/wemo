include $(TOPDIR)/rules.mk

PKG_NAME:=em35x_uart_test
PKG_RELEASE:=1

#PKG_SOURCE:=$(PKG_NAME).tar.gz
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
#PKG_MD5SUM:=5921c765c856582ee1b6d446513d76b5

include $(INCLUDE_DIR)/package.mk

TARGET_CFLAGS += -fpic

define Package/em35x_uart_test
	SECTION:=utils
	CATEGORY:=WNC utils
	DEFAULT:=n
	TITLE:=Zigbee UART Test
	MENU:=1
endef

define Package/em35x_uart_test/description
	Daemon for Zigbee UART Test.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
#	$(call Build/Compile/Default)
	$(MAKE) -C $(PKG_BUILD_DIR) -f $(PKG_BUILD_DIR)/Makefile \
		ROOTBINDIR="$(PKG_BUILD_DIR)" \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) -Wall" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/em35x_uart_test/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/uart-test-1 $(1)/usr/sbin/uart-test-1
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/uart-test-2 $(1)/usr/sbin/uart-test-2
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/uart-test-3 $(1)/usr/sbin/uart-test-3
endef

$(eval $(call BuildPackage,em35x_uart_test))

