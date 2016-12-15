include $(TOPDIR)/rules.mk

PKG_NAME    := pmortemd
PKG_RELEASE := 1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

TARGET_CFLAGS := 

define SUBMAKE_MK
all: pmortemd libpmortem.a crasher

pmortemd: pmortemd.o

libpmortem.a: pmortem.c
	$$(CC) $$(CFLAGS) -fPIC -c -o $$(subst .c,.o,$$<) $$<
	$$(AR) r $$@ $$(subst .c,.o,$$<)
	$$(RM) $$(subst .c,.o,$$<)

crasher: LDFLAGS := -lpmortem
crasher: LDLIBS := -L.
crasher: pmtest.o nop.o
	$$(CC) $$(LDLIBS) -o $$@ $$^ $$(LDFLAGS)
endef
export SUBMAKE_MK
export VPATH  := $(CURDIR)/src

define Package/pmortemd
  CATEGORY := Belkin
  SUBMENU  := Utilities
  SECTION  := utils
  TITLE    := Program for reporting another process\'s stack on abort.
endef

define Package/pmortemd/description
Daemon for reporting a process\'s stack when it abort.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$SUBMAKE_MK" > $(PKG_BUILD_DIR)/Makefile
endef

define Package/pmortemd/install
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/pmortemd $(1)/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(CURDIR)/src/pmortemd.init $(1)/etc/init.d/pmortemd
endef

define Build/InstallDev
	$(INSTALL_DIR) $(STAGING_DIR)/include
	$(CP) -fa src/pmortem.h $(STAGING_DIR)/include
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libpmortem.a $(1)/usr/lib
endef

$(eval $(call BuildPackage,pmortemd))
