include $(TOPDIR)/rules.mk

PKG_NAME:=zigbee_api
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export WEMO_LIB
export OUTPUT := libzigbee_api.so
export OBJS := $(patsubst %.c,%.o,$(notdir $(wildcard src/*.cpp) $(wildcard src/*.c)))
export VPATH := $(CURDIR)/src

TARGET_CFLAGS += -fpic

define Package/zigbee_api
  CATEGORY	:= Belkin
  SUBMENU	:= Libraries
  SECTION	:= libs
  TITLE		:= ZigBee library
endef

define Package/zigbee_api/description
	interface lib to zigbee daemon
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_LIB" > $(PKG_BUILD_DIR)/Makefile
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/usr/lib
	$(CP) src/zbIPC.h $(STAGING_DIR)/include
	$(CP) src/zigbee_api.h $(STAGING_DIR)/include
	$(CP) src/cluster-id.h $(STAGING_DIR)/include
endef

define Package/zigbee_api/install
	$(INSTALL_DIR) $(1)/lib
	$(CP) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/lib/
endef

$(eval $(call BuildPackage,zigbee_api))
