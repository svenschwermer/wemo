include $(TOPDIR)/rules.mk

PKG_NAME:=WeMo_Insight
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
TARGET_LDFLAGS += -lWemoDB -lUPnPHndlr -lPluginHndlr -linsight
TARGET_LDFLAGS += -lupnp -lthreadutil -lixml -lpthread -lssl -lcrypto
TARGET_LDFLAGS += -lcurl -lcares -lm
TARGET_LDFLAGS += -lmxml -lsqlite3
TARGET_LDFLAGS += -lpmortem

define Package/WeMo_Insight
  CATEGORY := Belkin
  SUBMENU  := Apps
  SECTION  := utils
  TITLE    := WeMo Insight App
  DEPENDS  += +UPnP +libpthread +libcares +libupnp +libuuid
  DEPENDS  += +HalInsight
  DEPENDS  += +NatClient
ifeq ($(CONFIG_TARGET_rt288x_WemoInsightCR),)
  KCONFIG  += CONFIG_WEMO_FLASH_OPTION_1=y
else
  KCONFIG  += CONFIG_WEMO_FLASH_OPTION_2=y
endif
  KCONFIG  += CONFIG_MTD_ROOTFS_ROOT_DEV=y
  KCONFIG  += CONFIG_MTD_ROOTFS_SPLIT=y
  KCONFIG  += CONFIG_ROOTFS_IN_FLASH_NO_PADDING=y
  KCONFIG  += CONFIG_RT2880_ROOTFS_IN_FLASH=y
  KCONFIG  += CONFIG_RALINK_GPIO_INSIGHT=y
  KCONFIG  += CONFIG_RF_OFFSET=0x00040000
  KCONFIG  += CONFIG_NVRAM_OFFSET=0x00fe0000
  KCONFIG  += CONFIG_BLK_DEV_RAM_SIZE=8192
  KCONFIG  += CONFIG_SCSI=y
  KCONFIG  += CONFIG_SCSI_PROC_FS=y
  KCONFIG  += CONFIG_SCSI_MULTI_LUN=y
ifeq ($(CONFIG_TARGET_rt288x_WemoInsightCR),)
  KCONFIG  += CONFIG_FAT_FS=y
  KCONFIG  += CONFIG_MSDOS_FS=y
endif
  KCONFIG  += CONFIG_VFAT_FS=y
  KCONFIG  += CONFIG_FAT_DEFAULT_CODEPAGE=437
  KCONFIG  += CONFIG_NLS=y
ifeq ($(CONFIG_TARGET_rt288x_WemoInsightCR),)
  KCONFIG  += CONFIG_NLS_DEFAULT="iso8859-1"
  KCONFIG  += CONFIG_FAT_DEFAULT_IOCHARSET="iso8859-1"
else
  KCONFIG  += CONFIG_NLS_DEFAULT="utf8"
  KCONFIG  += CONFIG_FAT_DEFAULT_IOCHARSET="utf8"
endif
  KCONFIG  += CONFIG_NLS_CODEPAGE_437=y
  KCONFIG  += CONFIG_NLS_ISO8859_1=y
  KCONFIG  += CONFIG_WIFI_DRIVER=y
  KCONFIG  += CONFIG_FIRST_IF_RT2860=y
  KCONFIG  += CONFIG_SECOND_IF_NONE=y
  KCONFIG  += CONFIG_RT_FIRST_CARD=2860
  KCONFIG  += CONFIG_RT_FIRST_IF_RF_OFFSET=0x40000
  KCONFIG  += CONFIG_WEMO_FLASH_OPTION_USB_ENABLE=y
  KCONFIG  += CONFIG_USB_ARCH_HAS_HCD=y
  KCONFIG  += CONFIG_USB_ARCH_HAS_OHCI=y
  KCONFIG  += CONFIG_USB_ARCH_HAS_EHCI=y
  KCONFIG  += CONFIG_USB=y
  KCONFIG  += CONFIG_USB_EHCI_HCD=y
  KCONFIG  += CONFIG_RT3XXX_EHCI=y
  KCONFIG  += CONFIG_USB_OHCI_HCD=y
  KCONFIG  += CONFIG_RT3XXX_OHCI=y
  KCONFIG  += CONFIG_USB_OHCI_LITTLE_ENDIAN=y
  KCONFIG  += CONFIG_USB_STORAGE=y
  KCONFIG  += CONFIG_USB_ACM=y
  KCONFIG  += CONFIG_HFS_FS=y
  KCONFIG  += CONFIG_HFSPLUS_FS=y
  KCONFIG  += CONFIG_EFS_FS=y
  KCONFIG  += CONFIG_PACKAGE_kmod-usb-storage=y
  KCONFIG  += CONFIG_SCSI=y
  KCONFIG  += CONFIG_BLK_DEV_SD=y
  KCONFIG  += CONFIG_HOTPLUG=y
  KCONFIG  += CONFIG_PROC_KCORE=y
  KCONFIG  += CONFIG_VFAT_FS=y
  KCONFIG  += CONFIG_NTFS_FS=y
ifeq ($(CONFIG_TARGET_rt288x_WemoInsightCR),)
  KCONFIG  += CONFIG_RT2860V2_AP_ED_MONITOR=y
endif
endef

define Package/WeMo_Insight/description
	WeMo Insight primary application
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_APP" > $(PKG_BUILD_DIR)/Makefile
endef

define Build/Compile
	$(MAKE_VARS) $(MAKE) -C $(PKG_BUILD_DIR)/$(MAKE_PATH) $(MAKE_FLAGS)
	@echo $(shell date "+%x %X") > $(PKG_BUILD_DIR)/ver.txt
ifeq ($(CONFIG_TARGET_rt288x_WemoInsightCR),)
ifeq ($(CONFIG_WEMO_PRODUCTION_IMAGE),)
	@echo WeMo_WW_2.00.$(shell $(TOPDIR)/scripts/revision.sh).EVT-OWRT-Insight >> $(PKG_BUILD_DIR)/ver.txt
else
	@echo WeMo_WW_2.00.$(shell $(TOPDIR)/scripts/revision.sh).PVT-OWRT-Insight >> $(PKG_BUILD_DIR)/ver.txt
endif
else
ifeq ($(CONFIG_WEMO_PRODUCTION_IMAGE),)
	@echo WeMo_WW_2.00.$(shell $(TOPDIR)/scripts/revision.sh).EVT-OWRT-InsightV2 >> $(PKG_BUILD_DIR)/ver.txt
else
	@echo WeMo_WW_2.00.$(shell $(TOPDIR)/scripts/revision.sh).PVT-OWRT-InsightV2 >> $(PKG_BUILD_DIR)/ver.txt
endif
endif
endef

define Package/WeMo_Insight/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OUTPUT) $(1)/sbin/
	$(INSTALL_BIN) ./files/startWemo.sh $(1)/sbin/
	$(INSTALL_DIR) $(1)/sbin/web/
	$(INSTALL_BIN) ./target/eventservice.xml $(1)/sbin/web/.
	$(INSTALL_BIN) ./target/Insightsetup.xml $(1)/sbin/web/setup.xml
	$(INSTALL_DIR) $(1)/etc/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/wemo.init $(1)/etc/init.d/wemo
	$(INSTALL_BIN) target/Insight.png $(1)/etc/icon.jpg
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ver.txt $(1)/etc

endef

$(eval $(call BuildPackage,WeMo_Insight))
