include $(TOPDIR)/rules.mk

PKG_NAME    := DataBase
PKG_RELEASE := 1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export WEMO_LIB
export OUTPUT := libWemoDB.so
export OBJS := $(patsubst %.c,%.o,$(notdir $(wildcard src/*.cpp) $(wildcard src/*.c)))
export VPATH := $(CURDIR)/src

EXTRA_CFLAGS += -fpic

define Package/DataBase
	CATEGORY := Belkin
	SUBMENU  := Libraries
	SECTION  := libs
	TITLE    := DataBase
	DEPENDS  := +sqlite +belkin_api
endef

define Package/DataBase/description
	WeMo application data base library.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_LIB" > $(PKG_BUILD_DIR)/Makefile
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/usr/lib
	$(CP) -fa src/WemoDB.h $(STAGING_DIR)/include
endef

define Package/DataBase/install
	$(INSTALL_DIR) $(1)/lib
	$(CP) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/lib/
endef

$(eval $(call BuildPackage,DataBase))
