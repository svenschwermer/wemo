include $(TOPDIR)/rules.mk

PKG_NAME:=WeMo_Light
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
TARGET_LDFLAGS += -lupnp -lthreadutil -lixml -lpthread -lssl -lcrypto
TARGET_LDFLAGS += -lcurl -lcares -lm
TARGET_LDFLAGS += -lmxml -lsqlite3
TARGET_LDFLAGS += -lpmortem

define Package/WeMo_Light
  CATEGORY := Belkin
  SUBMENU  := Apps
  SECTION  := utils
  TITLE    := WeMo Light App
  DEPENDS  += +UPnP +libpthread +libcares +libupnp +libuuid
  DEPENDS  += +NatClient
  KCONFIG  += CONFIG_WEMO_FLASH_OPTION_1=y
  KCONFIG  += CONFIG_MTD_ROOTFS_ROOT_DEV=y
  KCONFIG  += CONFIG_MTD_ROOTFS_SPLIT=y
  KCONFIG  += CONFIG_ROOTFS_IN_FLASH_NO_PADDING=y
  KCONFIG  += CONFIG_RT2880_ROOTFS_IN_FLASH=y
  KCONFIG  += CONFIG_RALINK_GPIO_LIGHT=y
  KCONFIG  += CONFIG_RF_OFFSET=0x00040000
  KCONFIG  += CONFIG_NVRAM_OFFSET=0x00fe0000
  KCONFIG  += CONFIG_WIFI_DRIVER=y
  KCONFIG  += CONFIG_FIRST_IF_RT2860=y
  KCONFIG  += CONFIG_SECOND_IF_NONE=y
  KCONFIG  += CONFIG_RT_FIRST_CARD=2860
  KCONFIG  += CONFIG_RT_FIRST_IF_RF_OFFSET=0x40000
  KCONFIG  += CONFIG_RT2860V2_AP_ED_MONITOR=y  
endef

define Package/WeMo_Light/description
	WeMo Light primary application
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_APP" > $(PKG_BUILD_DIR)/Makefile
endef

define Build/Compile
	$(MAKE_VARS) $(MAKE) -C $(PKG_BUILD_DIR)/$(MAKE_PATH) $(MAKE_FLAGS)
	@echo $(shell date "+%x %X") > $(PKG_BUILD_DIR)/ver.txt
ifeq ($(CONFIG_WEMO_PRODUCTION_IMAGE),)
	@echo WeMo_WW_2.00.$(shell $(TOPDIR)/scripts/revision.sh).DVT-OWRT-LS >> $(PKG_BUILD_DIR)/ver.txt
else
	@echo WeMo_WW_2.00.$(shell $(TOPDIR)/scripts/revision.sh).PVT-OWRT-LS >> $(PKG_BUILD_DIR)/ver.txt
endif
endef

define Package/WeMo_Light/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OUTPUT) $(1)/sbin/
	$(INSTALL_BIN) ./files/startWemo.sh $(1)/sbin/
	$(INSTALL_DIR) $(1)/sbin/web/
	$(INSTALL_BIN) ./target/eventservice.xml $(1)/sbin/web/.
	$(INSTALL_BIN) ./target/Lightsetup.xml $(1)/sbin/web/setup.xml
	$(INSTALL_DIR) $(1)/etc/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/wemo.init $(1)/etc/init.d/wemo
	$(INSTALL_BIN) target/Light.png $(1)/etc/icon.jpg
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ver.txt $(1)/etc
endef

$(eval $(call BuildPackage,WeMo_Light))
