include $(TOPDIR)/rules.mk

PKG_NAME:=HalInsight
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/belkin.mk
include $(INCLUDE_DIR)/belkin_diag.mk

define INSIGHT_DEMON
all: insightd
insightd: LDFLAGS += -L. -linsight -lbelkin_api -lbelkin_nvram -pthread -ldl -lm -ldmalloc -lz
insightd: insightd.o $$(OUTPUT)
	$$(CC) -o $$@ $$^ $$(LDFLAGS)
endef

export WEMO_LIB
export INSIGHT_DEMON
export OUTPUT := libinsight.so 
export OBJS := $(patsubst %.c,%.o,$(notdir $(filter-out src/insightd.c,$(wildcard src/*.c))))
export VPATH := $(CURDIR)/src

EXTRA_CFLAGS += -fpic

define Package/HalInsight
  CATEGORY	:= Belkin
  SUBMENU	:= Libraries
  SECTION	:= libs
  TITLE		:= HalInsight Demon
  DEPENDS	+= +WeMo_Core +belkin_api
endef

define Package/HalInsight/description
	WeMo_Insight HAL library.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	echo "$$$$INSIGHT_DEMON" >> $(PKG_BUILD_DIR)/Makefile
	echo "$$$$WEMO_LIB" >> $(PKG_BUILD_DIR)/Makefile
endef

define Build/Compile
	echo MAKE_VARS: $(MAKE_VARS)
	echo MAKE_FLAGS: $(MAKE_VARS)
	$(MAKE_VARS) $(MAKE) -C $(PKG_BUILD_DIR)/$(MAKE_PATH) \
		$(MAKE_FLAGS) $(OUTPUT)
	$(MAKE_VARS) $(MAKE) -C $(PKG_BUILD_DIR)/$(MAKE_PATH) \
		$(MAKE_FLAGS) insightd
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libinsight.so* $(1)/usr/lib
	$(INSTALL_BIN) src/insight.h $(STAGING_DIR)/include
endef

define Package/HalInsight/install
	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libinsight.so* $(1)/lib/
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/insightd $(1)/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/insightd.init $(1)/etc/init.d/insightd
endef

$(eval $(call BuildPackage,HalInsight))
