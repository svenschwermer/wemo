include $(TOPDIR)/rules.mk

PKG_NAME:=belkin_nvram_bd
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

LIBNAME := libbelkin_nvram.so

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk
include $(INCLUDE_DIR)/belkin_diag.mk


define Package/belkin_nvram_bd
  CATEGORY := Belkin
  SUBMENU  := Libraries
  SECTION  := libs
  TITLE    := Belkin hardware NVRAM Library
  DEPENDS  := +zlib +librt
  $(call BELKIN_DIAG_DEPENDS)
endef

define Package/belkin_nvram_bd/description
	Library to write to NVRAM flash partition
endef


define Build/Prepare
  $(call BELKIN_DIAG_PREP)
endef

define Build/Compile
	$(MAKE) -C src OBJDIR=$(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CPPFLAGS="-I$(STAGING_DIR)/usr/include -I$(STAGING_DIR)/include" \
		LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(LIBNAME) $(1)/lib
	$(INSTALL_DIR) $(1)/include
	$(INSTALL_BIN) src/libnvram.h $(1)/include
endef

define Package/belkin_nvram_bd/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/nvram $(1)/sbin/
	$(LN) -sf nvram $(1)/sbin/nvram_get
	$(LN) -sf nvram $(1)/sbin/nvram_set
	$(LN) -sf nvram $(1)/sbin/nvramd
	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(LIBNAME) $(1)/lib/
endef

$(eval $(call BuildPackage,belkin_nvram_bd))
