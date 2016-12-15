include $(TOPDIR)/rules.mk

PKG_NAME:=netcam_nvram
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk
include $(INCLUDE_DIR)/belkin_diag.mk

define Package/netcam_nvram
  CATEGORY := Belkin
  SUBMENU  := Libraries
  SECTION  := libs
  TITLE    := Seedonk hardware NVRAM Library
endef

define Package/netcam_nvram/description
	Library to write to NVRAM flash partition
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$NVRAM_APP" >> $(PKG_BUILD_DIR)/Makefile
	echo "$$$$WEMO_LIB" >> $(PKG_BUILD_DIR)/Makefile
	$(call BELKIN_DIAG_PREP)
endef

define Build/Compile
	echo $(PKG_NAME) nothing to be done.
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/lib
ifeq ($(CONFIG_NETCAM_NetCam),y)
	$(INSTALL_BIN) files/sdkNetCam/libnvram-0.9.28.so $(1)/lib/
endif
ifeq ($(CONFIG_NETCAM_HD_V1),y)
	$(INSTALL_BIN) files/sdkNetCam/libnvram-0.9.28.so $(1)/lib/
endif
ifeq ($(CONFIG_NETCAM_HD_V2),y)
	$(INSTALL_BIN) files/sdkNetCamHDv2/libnvram-0.9.28.so $(1)/lib/
endif
ifeq ($(CONFIG_NETCAM_LS),y)
	$(INSTALL_BIN) files/sdkNetCamHDv2/libnvram-0.9.28.so $(1)/lib/
endif
	ln -sf libnvram-0.9.28.so $(1)/lib/libnvram.so

	$(INSTALL_DIR) $(1)/include
ifeq ($(CONFIG_NETCAM_NetCam),y)
	$(CP) files/sdkNetCam/nvram.h $(1)/include/
endif
ifeq ($(CONFIG_NETCAM_HD_V1),y)
	$(CP) files/sdkNetCam/nvram.h $(1)/include/
endif
ifeq ($(CONFIG_NETCAM_HD_V2),y)
	$(CP) files/sdkNetCamHDv2/nvram.h $(1)/include/nvram_hd.h
endif
ifeq ($(CONFIG_NETCAM_LS),y)
	$(CP) files/sdkNetCamHDv2/nvram.h $(1)/include/nvram_hd.h
endif
endef

define Package/netcam_nvram/install
	$(INSTALL_DIR) $(1)/lib
ifeq ($(CONFIG_NETCAM_NetCam),y)
	$(INSTALL_BIN) files/sdkNetCam/libnvram-0.9.28.so $(1)/lib/
endif
ifeq ($(CONFIG_NETCAM_HD_V1),y)
	$(INSTALL_BIN) files/sdkNetCam/libnvram-0.9.28.so $(1)/lib/
endif
ifeq ($(CONFIG_NETCAM_HD_V2),y)
	$(INSTALL_BIN) files/sdkNetCamHDv2/libnvram-0.9.28.so $(1)/lib/
endif
ifeq ($(CONFIG_NETCAM_LS),y)
	$(INSTALL_BIN) files/sdkNetCamHDv2/libnvram-0.9.28.so $(1)/lib/
endif
	ln -sf libnvram-0.9.28.so $(1)/lib/libnvram.so
endef

$(eval $(call BuildPackage,netcam_nvram))
