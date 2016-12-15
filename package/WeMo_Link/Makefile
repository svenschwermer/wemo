include $(TOPDIR)/rules.mk

PKG_NAME:=WeMo_Link
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export WEMO_APP
export OUTPUT := wemoApp
export OBJS := $(patsubst %.c,%.o,$(notdir $(wildcard src/*.cpp) $(wildcard src/*.c)))
export VPATH := $(CURDIR)/src

TARGET_CFLAGS +=  -I$(STAGING_DIR)/usr/include/upnp
TARGET_LDFLAGS += -lbelkin_nvram -lbelkin_api -L$(STAGING_DIR)/usr/lib -lz
TARGET_LDFLAGS += -lAddlInfra -lNetworkControl -lDeviceControl -lRemoteAccess
TARGET_LDFLAGS += -lWemoDB -lUPnPHndlr -lPluginHndlr
TARGET_LDFLAGS += -lsysevent -lulog -lsubdevice -lzigbee_api
TARGET_LDFLAGS += -lupnp -lthreadutil -lixml -lpthread -lssl -lcrypto
TARGET_LDFLAGS += -lcurl -lcares -lm -lrt
TARGET_LDFLAGS += -lmxml -lsqlite3
TARGET_LDFLAGS += -luuid
TARGET_LDFLAGS += -lpmortem

define Package/WeMo_Link
  CATEGORY := Belkin
  SUBMENU  := Apps
  SECTION  := utils
  TITLE    := WeMo Link App
  DEPENDS  += +UPnP +libpthread +libcares +libupnp +libuuid
  DEPENDS  += +libpjnath +libuuid
  DEPENDS  += +sysevent +ulog +subdevice +zigbee_api
  DEPENDS  += +NatClient
  KCONFIG  += CONFIG_WEMO_FLASH_OPTION_2=y
  KCONFIG  += CONFIG_RF_OFFSET=0x00040000
  KCONFIG  += CONFIG_NVRAM_OFFSET=0x00fe0000
  KCONFIG  += CONFIG_FLASH_SIZE=16
  KCONFIG  += CONFIG_FLASH_ERASE_SIZE=4096
  KCONFIG  += CONFIG_UBOOT_SIZE=128
  KCONFIG  += CONFIG_KERNEL_PARTITION_SIZE=1024
  KCONFIG  += CONFIG_USER_PARTITION_SIZE=1280
  KCONFIG  += # CONFIG_RT2880_ROOTFS_IN_RAM is not set
  KCONFIG  += CONFIG_RT2880_ROOTFS_IN_FLASH=y
  KCONFIG  += CONFIG_ROOTFS_IN_FLASH_NO_PADDING=y
  KCONFIG  += CONFIG_BLK_DEV_RAM_SIZE=8192
  KCONFIG  += CONFIG_WIFI_DRIVER=y
  KCONFIG  += CONFIG_FIRST_IF_RT2860=y
  KCONFIG  += CONFIG_SECOND_IF_NONE=y
  KCONFIG  += CONFIG_RT2860V2_AP=m
  KCONFIG  += CONFIG_RT2860V2_AP_LED=y
  KCONFIG  += CONFIG_RT2860V2_AP_WSC_V2=y
  KCONFIG  += CONFIG_RT2860V2_AP_MBSS=y
  KCONFIG  += CONFIG_NEW_MBSSID_MODE=y
  KCONFIG  += CONFIG_RT2860V2_AP_80211N_DRAFT3=y
  KCONFIG  += CONFIG_RT2860V2_AP_RTMP_INTERNAL_TX_ALC=y
  KCONFIG  += CONFIG_RT_FIRST_CARD=2860
  KCONFIG  += CONFIG_RT_FIRST_IF_RF_OFFSET=0x40000
  KCONFIG  += CONFIG_MTD_ROOTFS_SPLIT=y
  KCONFIG  += CONFIG_MTD_ROOTFS_ROOT_DEV=y
  KCONFIG  += CONFIG_RALINK_GPIO_LINK=y
endef

define Package/WeMo_Link/description
	WeMo Link primary application
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_APP" > $(PKG_BUILD_DIR)/Makefile
endef

define Build/Compile
	$(MAKE_VARS) $(MAKE) -C $(PKG_BUILD_DIR)/$(MAKE_PATH) $(MAKE_FLAGS)
	@echo $(shell date "+%x %X") > $(PKG_BUILD_DIR)/ver.txt
ifeq ($(CONFIG_WEMO_PRODUCTION_IMAGE),)
	@echo WeMo_WW_2.00.$(shell $(TOPDIR)/scripts/revision.sh).DVT-OWRT-Link >> $(PKG_BUILD_DIR)/ver.txt
else
	@echo WeMo_WW_2.00.$(shell $(TOPDIR)/scripts/revision.sh).PVT-OWRT-Link >> $(PKG_BUILD_DIR)/ver.txt
endif
endef

define Package/WeMo_Link/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OUTPUT) $(1)/sbin/
	$(INSTALL_BIN) ./files/startWemo.sh $(1)/sbin/
	$(INSTALL_BIN) ./target/zbdmonitor.sh $(1)/sbin/
	$(INSTALL_DIR) $(1)/sbin/web
	$(CP) ./target/bridgesetup.xml $(1)/sbin/web/setup.xml
	$(CP) ./target/bridgerules.xml $(1)/sbin/web/
	$(CP) ./target/bridgeservice.xml $(1)/sbin/web/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/wemo.init $(1)/etc/init.d/wemo
	$(CP) target/icon.jpg $(1)/etc/
	$(CP) $(PKG_BUILD_DIR)/ver.txt $(1)/etc
endef

$(eval $(call BuildPackage,WeMo_Link))
