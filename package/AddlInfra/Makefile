include $(TOPDIR)/rules.mk

PKG_NAME := AddlInfra
PKG_RELEASE := 1

PKG_BUILD_DEPENDS:= openssl libupnp libcurl c-ares 

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export WEMO_LIB
export OUTPUT := libAddlInfra.so
export OBJS   := $(patsubst %.c,%.o,$(notdir $(wildcard src/*.cpp) $(wildcard src/*.c)))
export VPATH  := $(CURDIR)/src

EXTRA_CFLAGS  += -fpic -I$(CURDIR)/src -I$(STAGING_DIR)/usr/include/upnp
EXTRA_CFLAGS  += -D__HLOGS__ 
EXTRA_CFLAGS  += -pthread
#EXTRA_LDFLAGS += -lpthread -ldl -lm -lssl -lcrypto -lcares -lcurl -lz -lixml -lthreadutil
#EXTRA_LDFLAGS += -lbelkin_api 
#EXTRA_LDFLAGS += -pthread

LIBS += -L$(STAGING_DIR)/usr/lib

define Package/AddlInfra
	CATEGORY := Belkin
	SUBMENU  := Libraries
	SECTION  := libs
	TITLE    := AddlInfra
	DEPENDS  := +WeMo_Core +sqlite +belkin_api +ulog
	DEPENDS  += +libopenssl +curl +libcares +libupnp
endef

define Package/AddlInfra/description
	Wemo_Core additional infrastructure
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_LIB" > $(PKG_BUILD_DIR)/Makefile
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/usr/lib
endef

define Package/AddlInfra/install
	$(INSTALL_DIR) $(1)/lib
	$(CP) $(PKG_BUILD_DIR)/$(OUTPUT)* $(1)/lib/
endef

$(eval $(call BuildPackage,AddlInfra))
