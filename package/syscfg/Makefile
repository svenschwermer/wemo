#
# Copyright (C) 2009 Jo-Philipp Wich <xm@subsignal.org>
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=syscfg
PKG_RELEASE:=1
PKG_BUILD_DEPENDS:= ulog sysevent linux

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk

define SYSCFG
all: syscfg 
syscfg: LDFLAGS += -L. -L.. -lsyscfg -lulog -lpthread
syscfg: $(OBJS_C)
	$$(CC) -o $$@ $$^ $$(LDFLAGS)
endef

export WEMO_LIB
export SYSCFG
export OUTPUT := libsyscfg.so.1.0.0
export OBJS   := $(patsubst %.c,lib/%.o, $(notdir $(wildcard src/lib/*.cpp) $(wildcard src/lib/*.c)))
export OBJS_C := $(patsubst %.c,cmd/%.o, $(notdir $(wildcard src/cmd/*.cpp) $(wildcard src/cmd/syscfg.c)))
export VPATH  := $(CURDIR)/src 

define Package/syscfg
  CATEGORY	:= Belkin
  SUBMENU	:= Utility
  SECTION	:= utils
  TITLE		:= Linksys syscfg manipulation tool
  DEPENDS	+= +ulog 
  DEPENDS       += +sysevent 
  DEPENDS       += +linux
endef

EXTRA_CFLAGS += -Wall -g -fPIC -D__user="" -D_GNU_SOURCE -DSC_SYSV_SEM
EXTRA_LDFLAGS += -fPIC

define Package/syscfg/description
 This package contains an utility to manipulate syscfg database on Linksys based devices.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$SYSCFG" > $(PKG_BUILD_DIR)/Makefile
	echo "$$$$WEMO_LIB" >> $(PKG_BUILD_DIR)/Makefile
	$(CP) src/*  $(PKG_BUILD_DIR)/.
endef

define Build/Configure
endef

define Build/Compile
	$(CP) src/lib/*.h $(STAGING_DIR)/include
	$(MAKE_VARS) $(MAKE) -C $(PKG_BUILD_DIR)/$(MAKE_PATH) $(MAKE_FLAGS) $(OUTPUT)
	cd $(PKG_BUILD_DIR)/$(MAKE_PATH)/ && ln -s libsyscfg.so.1.0.0 libsyscfg.so 
	$(CP) src/lib/*.h $(STAGING_DIR)/include
	$(MAKE_VARS) $(MAKE) -C $(PKG_BUILD_DIR)/$(MAKE_PATH) $(MAKE_FLAGS) syscfg
endef


define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libsyscfg.so.1.0.0 $(1)/usr/lib/
	ln -s libsyscfg.so.1.0.0 $(1)/usr/lib/libsyscfg.so
endef

define Package/syscfg/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/syscfg $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libsyscfg.so.1.0.0 $(1)/lib/
	ln -s libsyscfg.so.1.0.0 $(1)/lib/libsyscfg.so
	ln -sf syscfg $(1)/usr/sbin/syscfg_create
	ln -sf syscfg $(1)/usr/sbin/syscfg_destroy
	ln -sf syscfg $(1)/usr/sbin/syscfg_format
	ln -sf syscfg $(1)/usr/sbin/syscfg_check
endef

$(eval $(call BuildPackage,syscfg))
