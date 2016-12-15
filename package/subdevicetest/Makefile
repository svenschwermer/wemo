include $(TOPDIR)/rules.mk

PKG_NAME:=subdevicetest
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export WEMO_APP
export OUTPUT := subdevicetest
export OBJS := $(patsubst %.c,%.o,$(notdir $(wildcard src/*.cpp) $(wildcard src/*.c)))
export VPATH := $(CURDIR)/src

TARGET_LDFLAGS += -lbelkin_nvram -lbelkin_api -L$(STAGING_DIR)/usr/lib -lz
TARGET_LDFLAGS += -lAddlInfra -lNetworkControl -lDeviceControl -lRemoteAccess
TARGET_LDFLAGS += -lWemoDB -lUPnPHndlr -lPluginHndlr -lpmortem
TARGET_LDFLAGS += -lsysevent -lulog -lsubdevice -lzigbee_api
TARGET_LDFLAGS += -lupnp -lthreadutil -lixml -lpthread -lssl -lcrypto
TARGET_LDFLAGS += -lcurl -lcares -lm -lrt
TARGET_LDFLAGS += -lsqlite3 -lmxml
TARGET_CFLAGS += -fpic

define Package/subdevicetest
  CATEGORY	:= Belkin
  SUBMENU	:= Utilities
  SECTION	:= utils
  TITLE		:= SubDevice Test
  DEPENDS	:= +subdevice +zigbee_api
endef

define Package/subdevicetest/description
	SubDevice Test Program
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_APP" > $(PKG_BUILD_DIR)/Makefile
endef

define Package/subdevicetest/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/subdevicetest $(1)/sbin/
endef

$(eval $(call BuildPackage,subdevicetest))

