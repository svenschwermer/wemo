include $(TOPDIR)/rules.mk

PKG_NAME    := belkin_diag
PKG_RELEASE := 1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

export VPATH := $(CURDIR)/src

define Package/belkin_diag
  CATEGORY := Belkin
  SUBMENU  := Libraries
  SECTION  := devel
  TITLE    := Belkin Diagnostics
  DEPENDS  := +libdmalloc
endef

define Package/belkin_diag/description
Run-time tools to help diagnose things like memory pointer errors.
This is enabled whenever the build target ends with -diag
endef

define Build/Compile
	@echo "belkin_diag Fake compile"
endef

define Build/Install
	@echo "belkin_diag Fake install"
endef

#define Build/clean
#	-rm $(PKG_BUILD_DIR)/Makefile
#endef
#
#define Build/Prepare
#	@echo "Starting belkin_diag Build/Prepare"
#	$(INSTALL_DIR) $(PKG_BUILD_DIR)
#	cp src/Makefile $(PKG_BUILD_DIR)
#	@echo "Done with belkin_diag Build/Prepare"
#endef
#
#define Build/InstallDev
#	@echo "Starting belkin_diag Build/InstallDev"
#	$(INSTALL_DIR) $(STAGING_DIR)/include
#	$(INSTALL_DATA) src/belkin_diag.h $(STAGING_DIR)/include
#	@echo "Done with belkin_diag Build/InstallDev"
#endef

$(eval $(call BuildPackage,belkin_diag))
