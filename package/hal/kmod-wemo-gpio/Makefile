#
# Copyright (C) 2008 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=wemo-gpio
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

GPIO_LOAD_PRIORITY=01
define KernelPackage/wemo-gpio
  CATEGORY:=Belkin
  SUBMENU:=HAL
  TITLE:=GPIO driver for wemo products
  ifdef CONFIG_WEMO_GPIO_SNS
    FILES:=$(PKG_BUILD_DIR)/sns_gpio.$(LINUX_KMOD_SUFFIX)
    AUTOLOAD:=$(call AutoLoad,$(GPIO_LOAD_PRIORITY),sns_gpio)
  endif
  ifdef CONFIG_WEMO_GPIO_BABYMON
    FILES:=$(PKG_BUILD_DIR)/babymon_gpio.$(LINUX_KMOD_SUFFIX)
    AUTOLOAD:=$(call AutoLoad,$(GPIO_LOAD_PRIORITY),babymon_gpio)
  endif
  ifdef CONFIG_WEMO_GPIO_INSIGHT
    FILES:=$(PKG_BUILD_DIR)/insight_gpio.$(LINUX_KMOD_SUFFIX)
    AUTOLOAD:=$(call AutoLoad,$(GPIO_LOAD_PRIORITY),insight_gpio)
  endif
  ifdef CONFIG_WEMO_GPIO_GUARDIAN
    FILES:=$(PKG_BUILD_DIR)/guardian_gpio.$(LINUX_KMOD_SUFFIX)
    AUTOLOAD:=$(call AutoLoad,$(GPIO_LOAD_PRIORITY),guardian_gpio)
  endif
  ifdef CONFIG_WEMO_GPIO_CROCKPOT
    FILES:=$(PKG_BUILD_DIR)/crockpot_gpio.$(LINUX_KMOD_SUFFIX)
    AUTOLOAD:=$(call AutoLoad,$(GPIO_LOAD_PRIORITY),crockpot_gpio)
  endif
endef

define KernelPackage/wemo-gpio/description
	Customized Ralink GPIO driver for wemo products
endef

define KernelPackage/wemo-gpio/config
source "$(SOURCE)/Config.in"
endef

EXTRA_KCONFIG:= CONFIG_WEMO_GPIO=m

ifdef CONFIG_WEMO_GPIO_SNS
	EXTRA_KCONFIG += CONFIG_WEMO_GPIO_SNS=m
endif

ifdef CONFIG_WEMO_GPIO_BABYMON
	EXTRA_KCONFIG += CONFIG_WEMO_GPIO_BABYMON=m
endif

ifdef CONFIG_WEMO_GPIO_INSIGHT
	EXTRA_KCONFIG += CONFIG_WEMO_GPIO_INSIGHT=m
endif

ifdef CONFIG_WEMO_GPIO_GUARDIAN
	EXTRA_KCONFIG += CONFIG_WEMO_GPIO_GUARDIAN=m
endif

ifdef CONFIG_WEMO_GPIO_CROCKPOT
	EXTRA_KCONFIG += CONFIG_WEMO_GPIO_CROCKPOT=m
endif

EXTRA_CFLAGS:= \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=m,%,$(filter %=m,$(EXTRA_KCONFIG)))) \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=y,%,$(filter %=y,$(EXTRA_KCONFIG)))) \
	-DWEMO_PRODUCT

MAKE_OPTS:= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)" \
	SUBDIRS="$(PKG_BUILD_DIR)" \
	EXTRA_CFLAGS="$(EXTRA_CFLAGS) $(WEMO_GPIO_MODULE)" \
	$(EXTRA_KCONFIG)

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(MAKE_OPTS) \
		modules
endef

$(eval $(call KernelPackage,wemo-gpio))
