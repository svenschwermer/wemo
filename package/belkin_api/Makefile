include $(TOPDIR)/rules.mk

PKG_NAME    := belkin_api
PKG_RELEASE := 1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export WEMO_LIB
export OUTPUT := libbelkin_api.so
export OBJS := $(patsubst %.c,%.o,$(notdir $(wildcard src/*.cpp) $(wildcard src/*.c)))
export VPATH := $(CURDIR)/src

EXTRA_CFLAGS += -fpic

define Package/belkin_api
  CATEGORY := Belkin
  SUBMENU  := Libraries
  SECTION  := libs
  TITLE    := Belkin WeMo API library
  DEPENDS  := @((PACKAGE_belkin_nvram_bd||PACKAGE_belkin_nvram_vm)||PACKAGE_netcam_nvram)
  DEPENDS  := +dmalloc 
endef

define Package/belkin_api/description
	OS and driver abstraction layer
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_LIB" > $(PKG_BUILD_DIR)/Makefile
	$(INSTALL_DIR) $(STAGING_DIR)/include
	$(INSTALL_DATA) src/belkin_diag.h $(STAGING_DIR)/include
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/usr/lib
	$(INSTALL_DATA) src/belkin_api.h $(STAGING_DIR)/include
	$(INSTALL_DATA) src/belkin_diag.h $(STAGING_DIR)/include
endef

define Package/belkin_api/install
	$(INSTALL_DIR) $(1)/lib
	$(CP) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/lib/
endef

$(eval $(call BuildPackage,belkin_api))
