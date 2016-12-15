include $(TOPDIR)/rules.mk

PKG_NAME:=WeMo_Jarden
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export WEMO_APP
export OUTPUT := wemoApp
export OBJS := $(patsubst %.c,%.o,$(notdir $(wildcard src/*.cpp) $(wildcard src/*.c)))
export VPATH := $(CURDIR)/src

TARGET_CFLAGS +=  -I$(STAGING_DIR)/usr/include/upnp
TARGET_LDFLAGS += -lbelkin_nvram -lbelkin_api -L$(STAGING_DIR)/usr/lib -lz
TARGET_LDFLAGS += -lAddlInfra -lNetworkControl -lDeviceControl -lRemoteAccess
TARGET_LDFLAGS += -lWemoDB -lUPnPHndlr -lPluginHndlr -lWasp
TARGET_LDFLAGS += -lupnp -lthreadutil -lixml -lpthread -lssl -lcrypto
TARGET_LDFLAGS += -lcurl -lcares -lm
TARGET_LDFLAGS += -lmxml -lsqlite3
TARGET_LDFLAGS += -lpjnath -lpjlib-util -lpj -luuid
TARGET_LDFLAGS += -lpmortem

define Package/WeMo_Jarden
  CATEGORY := Belkin
  SUBMENU  := Apps
  SECTION  := utils
  TITLE    := WeMo Jarden App
  DEPENDS  += +UPnP +wasp +libpthread +libcares +libupnp +libuuid
  DEPENDS  += +libpjnath
  KCONFIG  += CONFIG_WEMO_FLASH_SMART=y
endef

define Package/WeMo_Jarden/description
	WeMo Jarden primary application
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$WEMO_APP" > $(PKG_BUILD_DIR)/Makefile
endef

define Build/Compile
	$(MAKE_VARS) $(MAKE) -C $(PKG_BUILD_DIR)/$(MAKE_PATH) $(MAKE_FLAGS)
	@echo $(shell date "+%x %X") > $(PKG_BUILD_DIR)/ver.txt
	@echo WeMo_WW_2.00.$(shell $(TOPDIR)/scripts/revision.sh).DVT >> $(PKG_BUILD_DIR)/ver.txt
endef

define Package/WeMo_Jarden/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(OUTPUT) $(1)/sbin/
	$(INSTALL_BIN) ./files/startWemo.sh $(1)/sbin/
	$(INSTALL_DIR) $(1)/etc/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/wemo.init $(1)/etc/init.d/wemo
	$(CP) target/crockpot.jpg $(1)/etc/
	$(CP) $(PKG_BUILD_DIR)/ver.txt $(1)/etc
endef

$(eval $(call BuildPackage,WeMo_Jarden))
