include $(TOPDIR)/rules.mk

PKG_NAME:=powerMeterUartHandler
PKG_RELEASE:=1

# PKG_SOURCE:=$(PKG_NAME).tar.gz
# PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/powerMeterUartHandler
	SECTION:=utils
	CATEGORY:=WNC utils
	DEFAULT:=n
	TITLE:=powerMeterUartHandler utility
	MENU:=1
endef

define Package/powerMeterUartHandler/description
	powerMeterUartHandler is a for handling UART event from power meter
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(call Build/Compile/Default)
	$(MAKE) -C $(PKG_BUILD_DIR)/
endef

define Package/powerMeterUartHandler/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/powerMeterUartHandler $(1)/usr/sbin/powerMeterUartHandler
endef

$(eval $(call BuildPackage,powerMeterUartHandler))

