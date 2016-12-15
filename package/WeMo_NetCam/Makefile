include $(TOPDIR)/rules.mk

PKG_NAME    := WeMo_NetCam
PKG_RELEASE := 1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export WEMO_APP
export OUTPUT := wemoApp
export OBJS   := $(patsubst %.c,%.o,$(notdir $(wildcard src/App/*.cpp) $(wildcard src/App/*.c)))
export VPATH  := $(CURDIR)/src

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/upnp
TARGET_LDFLAGS += -lnvram -lbelkin_api -L$(STAGING_DIR)/usr/lib -lz
TARGET_LDFLAGS += -lAddlInfra -lNetworkControl -lDeviceControl -lRemoteAccess
TARGET_LDFLAGS += -lWemoDB -lUPnPHndlr -lPluginHndlr
TARGET_LDFLAGS += -lupnp -lthreadutil -lixml -lpthread -lssl -lcrypto
TARGET_LDFLAGS += -lcurl -lcares -lm
TARGET_LDFLAGS += -lmxml -lsqlite3
TARGET_LDFLAGS += -lpmortem
TARGET_LDFLAGS += -L$(PKG_BUILD_DIR)/profile -lnetcam_profile
TARGET_LDFLAGS += -L$(PKG_BUILD_DIR)/WeMoNetCamAPI -lWeMoNetCamAPI

define NETCAM_PROFILE
all: libnetcam_profile.so
PROFILE_LDFLAGS = $(LDFLAGS) -L. -L.. -lcurl -lbelkin_api -L$(STAGING_DIR)/usr/lib -lAddlInfra
PROFILE_CFLAGS = $(CFLAGS) -I$(STAGING_DIR)/include -I$(STAGING_DIR)/usr/include
libnetcam_profile.so: netcam_profile.o
	$$(CC) -shared $$(PROFILE_CFLAGS) $$(PROFILE_LDFLAGS) -o $$@ $$^
netcam_profile.o:
	$$(CC) -fPIC -c -Wall $$(PROFILE_CFLAGS) $(INCS) $(VPATH)/profile/netcam_profile.c -o $$@
clean :
	rm -f *.o *.so
endef

define NETCAM_API
all: libWeMoNetCamAPI.so
API_LDFLAGS =
API_CFLAGS = $(CFLAGS) -I$(STAGING_DIR)/include
libWeMoNetCamAPI.so: wemo_netcam_api.o wemo_netcam_ipc.o
	$$(CC) -shared -o $$@ $$^
wemo_netcam_api.o:
	$$(CC) -fPIC -c -Wall  $$(API_CFLAGS) $(INCS) $(VPATH)/WeMoNetCamAPI/wemo_netcam_api.c -o $$@
wemo_netcam_ipc.o:
	$$(CC) -fPIC -c -Wall  $$(API_CFLAGS) $(INCS) $(VPATH)/WeMoNetCamAPI/wemo_netcam_ipc.c -o $$@
clean :
	rm -f *.o *.so
endef

export NETCAM_PROFILE
export NETCAM_API

define Package/WeMo_NetCam
  CATEGORY := Belkin
  SUBMENU  := Apps
  SECTION  := utils
  TITLE    := WeMo Netcam App
  DEPENDS  += +UPnP +libpthread +libcares +libupnp +ulog
  KCONFIG  += CONFIG_WEMO_FLASH_OPTION_1=y
  KCONFIG  += CONFIG_MTD_ROOTFS_ROOT_DEV=y
  KCONFIG  += CONFIG_MTD_ROOTFS_SPLIT=y
  KCONFIG  += CONFIG_ROOTFS_IN_FLASH_NO_PADDING=y
  KCONFIG  += CONFIG_RT2880_ROOTFS_IN_FLASH=y
  KCONFIG  += CONFIG_RF_OFFSET=0x00040000
  KCONFIG  += CONFIG_NVRAM_OFFSET=0x00fe0000
endef

define Package/WeMo_NetCam/config
	source "$(SOURCE)/Config.in"
endef

define Package/WeMo_NetCam/description
	WeMo Netcam application
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/WeMoNetCamAPI
	echo "$$$$NETCAM_API" > $(PKG_BUILD_DIR)/WeMoNetCamAPI/Makefile
	$(CP) $(CURDIR)/src/WeMoNetCamAPI/*.h $(STAGING_DIR)/include
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/profile
	echo "$$$$NETCAM_PROFILE" > $(PKG_BUILD_DIR)/profile/Makefile
	$(CP) $(CURDIR)/src/profile/*.h $(STAGING_DIR)/include
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) $(CURDIR)/src/App/Plugin.c $(PKG_BUILD_DIR)
	echo "$$$$WEMO_APP" > $(PKG_BUILD_DIR)/Makefile
endef

define Build/Compile
	$(MAKE_VARS) $(MAKE) -C $(PKG_BUILD_DIR)/WeMoNetCamAPI/$(MAKE_PATH) $(MAKE_FLAGS)
	$(MAKE_VARS) $(MAKE) -C $(PKG_BUILD_DIR)/profile/$(MAKE_PATH) $(MAKE_FLAGS)
	$(MAKE_VARS) $(MAKE) -C $(PKG_BUILD_DIR)/$(MAKE_PATH) $(MAKE_FLAGS)
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/profile/libnetcam_profile.so $(1)/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/WeMoNetCamAPI/libWeMoNetCamAPI.so $(1)/lib
	$(CP) $(CURDIR)/src/WeMoNetCamAPI/*.h $(STAGING_DIR)/include
	$(CP) $(CURDIR)/src/profile/*.h $(STAGING_DIR)/include
endef

define Package/WeMo_NetCam/install
	$(INSTALL_DIR) $(1)/lib
	$(CP) $(PKG_BUILD_DIR)/profile/libnetcam_profile.so $(1)/lib/
	$(CP) $(PKG_BUILD_DIR)/WeMoNetCamAPI/libWeMoNetCamAPI.so $(1)/lib/
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wemoApp $(1)/sbin/
	$(INSTALL_BIN) files/wemo.sh $(1)/sbin/
ifeq ($(CONFIG_NETCAM_NetCam),y)
	  $(CP) files/sensor.jpg $(1)/sbin/sensor.jpg
	  $(CP) files/wemorc-NetCam $(1)/sbin/wemorc
endif
ifeq ($(CONFIG_NETCAM_HD_V1),y)
	  $(CP) files/sensor_hd.jpg $(1)/sbin/sensor.jpg
	  $(CP) files/wemorc-NetCam $(1)/sbin/wemorc
endif
ifeq ($(CONFIG_NETCAM_HD_V2),y)
	  $(CP) files/sensor_hd.jpg $(1)/sbin/sensor.jpg
	  $(CP) files/wemorc-HDv2 $(1)/sbin/wemorc
endif
ifeq ($(CONFIG_NETCAM_LS),y)
	  $(CP) files/sensor_ls.jpg $(1)/sbin/sensor.jpg
	  $(CP) files/wemorc-HDV2 $(1)/sbin/wemorc
endif
	$(INSTALL_BIN) files/wemo_restore.sh $(1)/sbin
endef

$(eval $(call BuildPackage,WeMo_NetCam))
