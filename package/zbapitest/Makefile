include $(TOPDIR)/rules.mk

PKG_NAME:=zbapitest
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export WEMO_APP
export OUTPUT := zbapitest
export OBJS := $(patsubst %.c,%.o,$(notdir $(wildcard src/*.cpp) $(wildcard src/*.c)))
export VPATH := $(CURDIR)/src

TARGET_LDFLAGS += -lpthread
TARGET_LDFLAGS += -lzigbee_api

TARGET_CFLAGS += -fpic

define Package/zbapitest
  CATEGORY	:= Belkin
  SUBMENU	:= Utilities
  SECTION	:= utils
  TITLE		:= zigbee_api test
  DEPENDS	:= +zigbee_api
endef

define Package/zbapitest/description
 zigbee_api test program
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_APP" > $(PKG_BUILD_DIR)/Makefile
endef

define Package/zbapitest/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/zbapitest $(1)/sbin/
endef

$(eval $(call BuildPackage,zbapitest))

