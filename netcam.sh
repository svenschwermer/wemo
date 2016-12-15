#!/bin/bash -x
BASE_DIR=`pwd`

install_tools() {
    make tools/install
    if [ $? != 0 ]; then
	exit 1
    fi
}

do_compile() {
    make package/$1/compile
    if [ $? != 0 ]; then
	echo "compiling again to see generate error log"
	make package/$1/compile V=99
	exit 1
    fi
}

compile_netcam() {
    do_compile ulog
    do_compile WeMo_NetCam
    do_compile NatClient
}

prepare_package() {
    export IPKG_TMP=$BASE_DIR/tmp/ipkg
    export IPKG_INSTROOT=$BASE_DIR/tmp/wemoApp
    export IPKG_CONF_DIR=$BASE_DIR/staging_dir/target-mipsel-linux_netcam/etc
    export IPKG_OFFLINE_ROOT=$BASE_DIR/tmp/wemoApp

    rm -rf $IPKG_INSTROOT
    mkdir $IPKG_INSTROOT
    packages="AddlInfra_1 belkin_api_1 DataBase_1 DeviceControl_1 Handlers_1 libcares_1.9.1-1 libcurl_7.29.0-1 NetworkControl_1 RemoteAccess_1 ulog_1 UPnP_1 WeMo_Core_1 WeMo_NetCam_1 zlib_1.2.3-5 pmortemd_1 libupnp_1.6.19-1 libuuid_1.41.11-1 libmxml_2.9-1 NatClient_1 sqlite_3080704-1"

    for pkg in $packages; do
	$BASE_DIR/scripts/ipkg -force-defaults -force-depends install $BASE_DIR/bin/rt288x/packages/"$pkg"_rt288x.ipk 2> /dev/null
    done

    rm -rf $BASE_DIR/tmp/wemoApp/usr/lib/opkg
    cp $BASE_DIR/package/WeMo_NetCam/files/Makefile $IPKG_INSTROOT
    mv $IPKG_INSTROOT/etc/devices.txt $IPKG_INSTROOT/sbin/
#	mkdir $IPKG_INSTROOT/sbin/cacerts
#	mv $IPKG_INSTROOT/etc/certs/BuiltinObjectToken-GoDaddyClass2CA.crt $IPKG_INSTROOT/sbin/cacerts/

    BUILD_TIME=`date "+%x %X"`
    echo $BUILD_TIME >> $IPKG_INSTROOT/sbin/ver.txt
    #TODO: make below dynamic
    if [ "$1" = "prod" ]; then
	echo WeMo_NetCam_WW_2.00.${WEMO_VERSION}.PVT >> $IPKG_INSTROOT/sbin/ver.txt
    else
	echo WeMo_NetCam_WW_2.00.${WEMO_VERSION}.DVT >> $IPKG_INSTROOT/sbin/ver.txt
    fi

    tar -C $BASE_DIR/tmp -czf $BASE_DIR/bin/rt288x/netcam-pkg.tar.gz wemoApp

    echo "netcam wemo software package available as bin/rt288x/netcam-pkg.tar.gz"
    cp -f $BASE_DIR/bin/rt288x/netcam-pkg.tar.gz $BASE_DIR/bin/${WEMO_DIST_PKG}
}

build_image_with_sdk() {
    rm -rf $BASE_DIR/build_dir/NetCam_SDK
    $BASE_DIR/package/WeMo_NetCam/extras/install_netcam_sdk $1
    if [ $1 = "sd" ]; then
	cp $BASE_DIR/package/WeMo_NetCam/extras/makedevlinks.sd $BASE_DIR/build_dir/NetCam_SDK/vendors/Ralink/RT5350/makedevlinks
    elif [ $1 = "hdv1" ]; then
	cp $BASE_DIR/package/WeMo_NetCam/extras/makedevlinks.hdv1 $BASE_DIR/build_dir/NetCam_SDK/vendors/Ralink/RT5350/makedevlinks
    elif [ $1 = "hdv2" ]; then
	cp $BASE_DIR/package/WeMo_NetCam/extras/makedevlinks.hdv2 $BASE_DIR/build_dir/NetCam_SDK/vendors/Ralink/MT7620/makedevlinks
    elif [ $1 = "ls" ]; then
	cp $BASE_DIR/package/WeMo_NetCam/extras/makedevlinks.hdv2 $BASE_DIR/build_dir/NetCam_SDK/vendors/Ralink/MT7620/makedevlinks
    fi
    $BASE_DIR/package/WeMo_NetCam/extras/netcam_build_mod install
    cd $BASE_DIR/build_dir/NetCam_SDK/user && tar xzf $BASE_DIR/bin/rt288x/netcam-pkg.tar.gz
    cd $BASE_DIR/build_dir/NetCam_SDK/ && PATH=/opt/buildroot-gcc342/bin:$PATH fakeroot make PROD=NetCam
    cp $BASE_DIR/build_dir/NetCam_SDK/tftpboot/$IMAGENAME $BASE_DIR/bin/$TARGET_IMAGENAME
}

WEMO_VERSION=`svn info | grep "Last Changed Rev:" | awk {' print $4 '}`
DATE_INFO=`date +%H%M%S`
if [ "$2" = "prod" ]; then
PREFIX=WeMo_NetCam_WW_2.00.${WEMO_VERSION}.PVT_${DATE_INFO}
else
PREFIX=WeMo_NetCam_WW_2.00.${WEMO_VERSION}.DVT_${DATE_INFO}
fi

IMAGENAME_PREFIX=${PREFIX}_uImage
WEMO_DISTNAME_PREFIX=${PREFIX}_distro

case "${1}" in
    sd)
	PRODUCT=NetCam
	IMAGENAME=NETCAM.img
	TARGET_IMAGENAME=${IMAGENAME_PREFIX}_NetCam
	WEMO_DIST_PKG=${WEMO_DISTNAME_PREFIX}_NetCam.tar.gz
	;;
    hdv1)
	PRODUCT=HD_V1
	IMAGENAME=NetCamHDv1.img
	TARGET_IMAGENAME=${IMAGENAME_PREFIX}_HDv1
	WEMO_DIST_PKG=${WEMO_DISTNAME_PREFIX}_HDv1.tar.gz
	;;
    hdv2)
	PRODUCT=HD_V2
	IMAGENAME=NetCamHDv2.img
	TARGET_IMAGENAME=${IMAGENAME_PREFIX}_HDv2
	WEMO_DIST_PKG=${WEMO_DISTNAME_PREFIX}_HDv2.tar.gz
	;;
    ls)
	PRODUCT=LS
	IMAGENAME=linksys_outdoor.img
	TARGET_IMAGENAME=${IMAGENAME_PREFIX}_LS
	;;
    *)
	echo "Usage: ${0} {sd|hdv1|hdv2|ls}" >&2
	echo "Usage: for production image ${0} {sd|hdv1|hdv2|ls} prod" >&2
	exit 1
	;;
esac

make netcam-config
echo "CONFIG_NETCAM_${PRODUCT}=y" >> .config

if [ "$2" = "prod" ]; then
    echo CONFIG_WEMO_PRODUCTION_IMAGE=y >> .config
fi

make -f Makefile defconfig
install_tools
compile_netcam
prepare_package $2
build_image_with_sdk $1

if [ ! -z "${JOB_NAME}" ]; then
  scp -i ~/westcost.pem -p $WORKSPACE/bin/${TARGET_IMAGENAME} ubuntu@50.18.177.100:/var/www/firmware/${JOB_NAME}/openwrt
  scp -i ~/westcost.pem -p $WORKSPACE/bin/${WEMO_DIST_PKG} ubuntu@50.18.177.100:/var/www/firmware/${JOB_NAME}/openwrt

  echo "http://app.xbcs.net/firmware/${JOB_NAME}/openwrt/${TARGET_IMAGENAME}"
  echo "http://app.xbcs.net/firmware/${JOB_NAME}/openwrt/${WEMO_DIST_PKG}"
else
    mkdir -p $BASE_DIR/output
    cp $BASE_DIR/bin/${TARGET_IMAGENAME} $BASE_DIR/output
    cp $BASE_DIR/bin/${WEMO_DIST_PKG} $BASE_DIR/output
fi

exit 0	
