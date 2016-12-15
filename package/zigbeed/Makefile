include $(TOPDIR)/rules.mk

PKG_NAME:=zigbeed
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

TARGET_CFLAGS += -fpic

define Package/zigbeed
  CATEGORY	:= Belkin
  SUBMENU	:= Deamons
  SECTION	:= utils
  TITLE		:= ZigBee daemon
  DEPENDS	:= +zigbee_api
endef

define Package/zigbeed/description
	ZigBee daemon
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) src/* $(PKG_BUILD_DIR)/.
endef

define Build/Configure
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) -f $(PKG_BUILD_DIR)/HA_gateway_NCP.mak \
		ROOTBINDIR="$(PKG_BUILD_DIR)" \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) -Wall" \
		LDFLAGS="$(TARGET_LDFLAGS)"
#	$(MAKE) -C $(PKG_BUILD_DIR) -f $(PKG_BUILD_DIR)/ezsp-uart-host.mak \
#		ROOTBINDIR="$(PKG_BUILD_DIR)" \
#		CC="$(TARGET_CC)" \
#		CFLAGS="$(TARGET_CFLAGS) -Wall" \
#		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/zigbeed/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/zigbeed $(1)/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/zigbeed.init $(1)/etc/init.d/zigbeed
#	$(INSTALL_DIR) $(1)/usr/sbin
#	$(INSTALL_BIN) $(PKG_BUILD_DIR)/uart-test-1 $(1)/usr/sbin/
#	$(INSTALL_BIN) $(PKG_BUILD_DIR)/uart-test-2 $(1)/usr/sbin/
#	$(INSTALL_BIN) $(PKG_BUILD_DIR)/uart-test-3 $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,zigbeed))

