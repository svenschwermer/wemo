include $(TOPDIR)/rules.mk

PKG_NAME:=subdevice
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export WEMO_LIB
export OUTPUT := libsubdevice.so
export OBJS := $(patsubst %.c,%.o,$(notdir $(wildcard src/*.cpp) $(wildcard src/*.c)))
export VPATH := $(CURDIR)/src

TARGET_CFLAGS += -fpic -I$(STAGING_DIR)/usr/include/upnp

define Package/subdevice
  CATEGORY	:= Belkin
  SUBMENU	:= Libraries
  SECTION	:= libs
  TITLE		:= Belkin SubDevice library
  DEPENDS	:= +zigbee_api +ulog
endef

define Package/subdevice/description
	SubDevice Interface library
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_LIB" > $(PKG_BUILD_DIR)/Makefile
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/usr/lib
	$(CP) src/subdevice.h $(STAGING_DIR)/include
endef

define Package/subdevice/install
	$(INSTALL_DIR) $(1)/lib
	$(CP) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/lib/
endef

$(eval $(call BuildPackage,subdevice))
