#!/bin/bash

svn merge ^/branches/backfire_smart_corewifi/packages/WeMo_Core/AddInfra/src package/AddlInfra/src
svn merge ^/branches/backfire_smart_corewifi/packages/WeMo_Core/DataBase/src package/DataBase/src
svn merge ^/branches/backfire_smart_corewifi/packages/WeMo_Core/Include package/WeMo_Core/Include
svn merge ^/branches/backfire_smart_corewifi/packages/WeMo_Core/DeviceControl/src package/DeviceControl/src
svn merge ^/branches/backfire_smart_corewifi/packages/WeMo_Core/NetworkControl/src package/NetworkControl/src
svn merge ^/branches/backfire_smart_corewifi/packages/WeMo_Core/UPnP/src package/UPnP/src
svn merge ^/branches/backfire_smart_corewifi/packages/WeMo_Core/RemoteAccess/src package/RemoteAccess/src
svn merge ^/branches/backfire_smart_corewifi/packages/WeMo_Core/Handlers/src package/Handlers/src
svn merge ^/branches/backfire_smart_corewifi/packages/WeMo_Core/WeMo_NetCam/src package/WeMo_NetCam/src

