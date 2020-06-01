#! /bin/sh
#

logfile=/tmp/update.log
FIRMWARE_NAME=firmware.bin
STRIP_FIRMWARE=firmware.strip

if [ ! -f $logfile ]; then
# run with logging
   sh -x $0 $1 > $logfile 2>&1
   status=$?
   cat $logfile > /dev/console
   sleep 5
   echo "Exiting with status $status" > /dev/console
   exit $status
fi

report_err() {
   echo $1 > $errfile
   echo $2 >> $errfile
   echo $3 >> $errfile
   echo "Error: $3"
}

errfile="/update.errs"

cd /tmp
rm -rf update

echo "Processing $1"
export GNUPGHOME=/root/.gnupg

# make sure the update file exists
if [ ! -f $1 ]; then
   report_err 5 2 "The specified update file '$1' does not exist."
   return 2
fi

# make sure the update is signed
gpg --import --ignore-time-conflict /root/.gnupg/WeMoPubKey.asc


# Proposed fix for WEMO-50242 and WEMO-50425
# Verify signature using Signature Chain method
success=false
wrong_key=false
status_code=""

# Loop until there is no signature found
while [ "$status_code" != "NODATA" ]; do
   echo "Checking next signature layer."
   # Check validity of signature, store detailed status in status.text
   gpg --verify --ignore-time-conflict --status-file status.txt "$1"
   # Read status.txt for status codes and set variables appropriately
   if grep -Fq "NO_PUBKEY" status.txt; then
      echo "Unrecognized signature found."
      status_code="NO_PUBKEY"
      wrong_key=true
   elif grep -Fq "VALIDSIG" status.txt; then
      success=true
      echo "Valid signature found."
      status_code="VALIDSIG"
   elif grep -Fq "NODATA" status.txt; then
      echo "No more signatures on the firmware."
      status_code="NODATA"
   else
      echo "Status checking failed."
      status_code="ERROR"
      break
   fi
   echo "$status_code"
   # Remove the signature if there is one
   if [ "$status_code" != "NODATA" ]; then
      ret=`image_tool -u $1`
	  echo "image_tool returns $ret"
   fi
done

# Error encountered using gpg
if [ "$status_code" = "ERROR" ]; then
   echo "Error checking gpg output status. Aborting firmware update."
   report_err 1 $status "Signature check encountered error."
   return 1
# Signature validated and all signatures were removed
elif $success && [ "$status_code" = "NODATA" ]; then
   echo "Valid signature found. Installing Firmware."
# No signature could be validated
else
   echo "No valid signature found. Aborting firmware update."
   report_err 1 $status "Signature test failed."
   return 1
fi


# verify belkin header and chksum _start
BELKIN_HDR=belkin.hdr
if $wrong_key; then
   size=`cat $1 | wc -c`
   skip_size=`expr $size - 256`
   dd if="$1" of="$BELKIN_HDR" skip="$skip_size" bs=1 count=256 > /dev/console
fi
magic_string="`cat $BELKIN_HDR | cut -b 1-9`"
hdr_version="`cat $BELKIN_HDR | cut -b 10-11`"
hdr_length="`cat $BELKIN_HDR | cut -b 12-16`"

sku_length="`cat $BELKIN_HDR | cut -b 17`"
sku_decimal_len=`printf "%d" 0x"$sku_length"`
sku_end=`expr 18 + $sku_decimal_len - 2`
sku_string="`cat $BELKIN_HDR | cut -b 18-$sku_end`"

img_cksum="`cat $BELKIN_HDR | cut -b 33-40`"
sign_type="`cat $BELKIN_HDR | cut -b 41`"
signer="`cat $BELKIN_HDR | cut -b 42-48`"

kernel_ofs="`cat $BELKIN_HDR | cut -b 50-56`"
rfs_ofs="`cat $BELKIN_HDR | cut -b 58-64`"

if [ "$magic_string" != ".BELKIN.." ]
then
	status=1
	report_err 3 $status  "Fail : verify magic string"
	return $status
fi

if $wrong_key; then
    crc1=`dd if="$1" bs=$skip_size count=1| cksum | cut -d' ' -f1`
else
    crc1=`cksum $1 | cut -d' ' -f1`
fi

hex_cksum=`printf "%08X" "$crc1"`
if [ "$img_cksum" != "$hex_cksum" ]
then
	status=1
	report_err 3 $status  "Fail : verify image checksum"
	return $status
fi

PRODUCT_FILE="/etc/product.name"
if [ -e $PRODUCT_FILE ]
then
	sku_in_rfs="`cat $PRODUCT_FILE`"
	if [ "$sku_in_rfs" != $sku_string ]
	then
           	if [ "$sku_in_rfs" == "WeMo_link" -a "$sku_string" == "LEDLight" ]
               	then
                	echo "Continue Update Link to LEDLight ($sku_in_rfs) to ($sku_string)"
                else
                	status=1
                        report_err 3 $status  "Fail : verify product name"
                        return $status
                fi
	fi
fi
# verify belkin header and chksum _end

mv $1 $FIRMWARE_NAME
state_1st_updated=1
state_2nd_updated=3	

mounted_mtd=`cat /proc/mtd | grep "rootfs_data" | cut -d':' -f1 | cut -f2 -d"d"`
case "$mounted_mtd" in
	3 | 6 )
		echo "Updating 'B' image"
		mtd_other=Firmware_2
		newstate=$state_2nd_updated
		newpart=1
		;;

	5 | 8 )
		echo "Updating 'A' image"
		mtd_other=Firmware_1
		newstate=$state_1st_updated
		newpart=0
		;;

	*)
		echo "Invalid Boot partition : Updating A' image by default"
		mtd_other=Firmware_1
		newstate=$state_1st_updated
		newpart=0
		;;
esac
killall -9 watchdog
echo "V" >/dev/watchdog

mtd write $FIRMWARE_NAME $mtd_other
status=$?
if [ $status -ne 0 ]; then
   echo "mtd write failed ($status)"
   # create a flag file for startWemo.sh so wemoApp won't be restarted
   # when it exits after initating a reboot
   touch /tmp/rebooting
   # The following is a fail safe in case wemoApp crashes before issuing a reboot.
   (sleep 60; reboot) &
   echo "Rebooting..."
   return $status
fi

fw_setenv bootstate $newstate

# bootpart is changed temporaily to get backward compatibility for WNC old bootloaders (SDK_1.0.00.002 ~ 004)
fw_setenv bootpart $newpart

check_boot=`fw_printenv -n check_boot`
if [ "${check_boot}x" != "0x" ]; then
   fw_setenv check_boot 0
fi

echo $1 > Belkin_settings/last_update

echo "Rebooting..."
# create a flag file for startWemo.sh so wemoApp won't be restarted
# when it exits after initating a reboot
touch /tmp/rebooting
# The following is a fail safe in case wemoApp crashes before issuing a reboot.
(sleep 60; reboot) &


echo "firmware_update.sh exiting"
return $status


