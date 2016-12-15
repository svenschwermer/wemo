#!/bin/bash

SOURCE=$1
SOURCE=${SOURCE:-Jarden_Merge}

svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/openwrt/AddlInfra/Makefile package/AddlInfra/Makefile
svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/Plugin/src/WeMo_Core/AddlInfra package/AddlInfra/src

svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/openwrt/DataBase package/DataBase
svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/Plugin/src/WeMo_Core/DataBase package/DataBase/src

svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/Plugin/src/WeMo_Core/Include package/WeMo_Core/Include

svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/openwrt/DeviceControl/Makefile package/DeviceControl/Makefile
svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/Plugin/src/WeMo_Core/DeviceControl package/DeviceControl/src

svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/openwrt/NetworkControl/Makefile package/NetworkControl/Makefile
svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/Plugin/src/WeMo_Core/NetworkControl package/NetworkControl/src

svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/openwrt/UPnP/Makefile package/UPnP/Makefile
svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/Plugin/src/WeMo_Core/UPnP package/UPnP/src

svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/openwrt/RemoteAccess/Makefile package/RemoteAccess/Makefile
svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/Plugin/src/WeMo_Core/RemoteAccess package/RemoteAccess/src

svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/openwrt/Handlers/Makefile package/Handlers/Makefile
svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/Plugin/src/WeMo_Core/Handlers package/Handlers/src

svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/openwrt/package/libpjnath package/libpjnath

svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/openwrt/WeMo_Jarden package/WeMo_Jarden
svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/Plugin/src/WeMo_Jarden/App package/WeMo_Jarden/src
svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/Plugin/src/WeMo_Jarden/target/crockpot.jpg package/WeMo_Jarden/target/crockpot.jpg

svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/Plugin/src/WeMo_Smart/App package/WeMo_Smart/src
svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/Plugin/src/WeMo_Smart/target/crockpot.jpg package/WeMo_Smart/target/crockpot.jpg

svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/openwrt/package/c-ares package/c-ares

svn merge ^/trunk/embedded-WeMo_2_0_${SOURCE}/openwrt/hal package/hal
