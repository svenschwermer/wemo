include $(TOPDIR)/rules.mk

PKG_NAME := DeviceControl
PKG_RELEASE := 1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export WEMO_LIB
export OUTPUT := libDeviceControl.so
export OBJS := $(patsubst %.c,%.o,$(notdir $(wildcard src/*.cpp) $(wildcard src/*.c)))
export VPATH := $(CURDIR)/src

TARGET_CFLAGS += -fpic -I$(STAGING_DIR)/usr/include/upnp

define Package/DeviceControl
  CATEGORY := Belkin
  SUBMENU  := Libraries
  SECTION  := libs
  TITLE    := DeviceControl
  DEPENDS  := +libupnp +NetworkControl +AddlInfra +WeMo_Core
  DEPENDS  += +PACKAGE_wasp:wasp
endef

define Package/DeviceControl/description
	WeMo device control library
endef

define Build/Prepare
	$(CP) ../UPnP/src/simulatedOccupancy.h $(STAGING_DIR)/include
	$(CP) ../UPnP/src/sunriset.h $(STAGING_DIR)/include
	$(CP) ../RemoteAccess/src/UDSClientHandler.h $(STAGING_DIR)/include
	$(CP) ../NatClient/src/natClient.h $(STAGING_DIR)/include
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_LIB" > $(PKG_BUILD_DIR)/Makefile
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/usr/lib
	$(CP) -fa src/{gpio,watchDog}.h $(STAGING_DIR)/include
endef

define Package/DeviceControl/install
	$(INSTALL_DIR) $(1)/lib
	$(CP) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/lib/
endef

$(eval $(call BuildPackage,DeviceControl))
