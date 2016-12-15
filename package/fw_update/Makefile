include $(TOPDIR)/rules.mk

PKG_NAME:=fw_update
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

SKU := $(shell sed -n '/^CONFIG_BUILD_SUFFIX=/{s/CONFIG_BUILD_SUFFIX="\([^"]\+\)"/\1/;s/_prod//;p}' $(TOPDIR)/.config)

define Package/fw_update
  CATEGORY	:= Belkin
  SUBMENU	:= Libraries
  SECTION	:= utils
  TITLE		:= firmware update scripts
  DEPENDS	+= +gnupg +uboot-envtools
endef

define Package/fw_update/description
	the scripts necessary to update firmware
endef

define Build/Prepare
endef

define Build/Compile
endef

define Package/fw_update/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) ./files/update_test.sh $(1)/sbin
	$(INSTALL_DIR) $(1)/root/.gnupg
ifeq ($(CONFIG_WEMO_PRODUCTION_IMAGE),)
	$(INSTALL_BIN) ./target.keys/* $(1)/root/.gnupg
	$(INSTALL_BIN) ./files/firmware_update.sh $(1)/sbin
else
	$(INSTALL_BIN) ./target.keys/WeMoPubKey.asc $(1)/root/.gnupg
	$(INSTALL_BIN) ./files/firmware_update_prod.sh $(1)/sbin/firmware_update.sh
endif
	$(INSTALL_DIR) $(1)/etc
	echo WeMo_$(SKU) > $(1)/etc/product.name
endef

$(eval $(call BuildPackage,fw_update))

