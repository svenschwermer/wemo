include $(TOPDIR)/rules.mk

PKG_NAME:=ha_zigbee_uartf
PKG_RELEASE:=1

#PKG_SOURCE:=$(PKG_NAME).tar.gz
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
#PKG_MD5SUM:=ea059914c4fc056d7429acc73df503b2

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

TARGET_CFLAGS += -fpic -lpthread

define Package/ha_zigbee_uartf
	SECTION:=utils
	CATEGORY:=WNC utils
	DEFAULT:=n
	TITLE:=Zigbee uart controller
	MENU:=1
endef

define Package/ha_zigbee_uartf/description
	Daemon for Zigbee uart controller.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) src/* $(PKG_BUILD_DIR)/.
endef

define Build/Compile
#	$(call Build/Compile/Default)
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -o $(PKG_BUILD_DIR)/ha_zigbee_uartf $(PKG_BUILD_DIR)/ha_zigbee_uartf.c
#	$(MAKE) -C $(PKG_BUILD_DIR)/
endef

define Package/ha_zigbee_uartf/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ha_zigbee_uartf $(1)/usr/sbin/ha_zigbee_uartf
endef

$(eval $(call BuildPackage,ha_zigbee_uartf))

