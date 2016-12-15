include $(TOPDIR)/rules.mk

PKG_NAME:=RemoteAccess
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

PKG_BUILD_DEPENDS:= libpjnath libcurl

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export WEMO_LIB
export OUTPUT := libRemoteAccess.so
export OBJS := $(patsubst %.c,%.o,$(notdir $(wildcard src/*.cpp) $(wildcard src/*.c)))
export VPATH := $(CURDIR)/src

EXTRA_CFLAGS += -fpic -I$(STAGING_DIR)/usr/include/upnp
ifneq ($(CONFIG_PACKAGE_WeMo_SNS),)
EXTRA_CFLAGS += -D__NFTOCLOUD__
endif

EXTRA_CFLAGS += -D_REENTRANT -DTHREADSAFE=1 -D__RINIT_M
EXTRA_CFLAGS += -D__WIRED_ETH__ -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0

EXTRA_LDFLAGS += -lbelkin_api -lpmortem -lupnp

define Package/RemoteAccess
  CATEGORY := Belkin
  SUBMENU  := Libraries
  SECTION  := libs
  TITLE    := RemoteAccess
#  DEPENDS  := +libmxml +Handlers +DataBase +AddlInfra +NetworkControl +pmortemd
  DEPENDS := +libmxml +DataBase +pmortemd +libupnp
  DEPENDS  += +PACKAGE_wasp:wasp
endef

define Package/RemoteAccess/description
	WeMo application remote access library.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_LIB" > $(PKG_BUILD_DIR)/Makefile
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/usr/lib
	$(INSTALL_BIN) src/insightRemoteAccess.h $(STAGING_DIR)/include
	$(INSTALL_DIR) $(1)/usr/include
	$(CP) src/UDSClientHandler.h $(1)/usr/include
endef

define Package/RemoteAccess/install
	$(INSTALL_DIR) $(1)/lib
	$(CP) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/lib/
endef

$(eval $(call BuildPackage,RemoteAccess))
