# Manage wemoApp, the WeMo application

# Load hardware specific configuration
source /sbin/wemorc

update_nvram_value()
{
        key=$1
        val=$2
        o_val=`nvram_get 2860 $key`
        if [ "$o_val" != "$val" ]; then
                nvram_set 2860 $key "$val"
        fi
}

# Load hardware specific configuration
source /sbin/wemorc

# Return 1 when in setup, 0 when not
read_setup_state() {
    gpio r $GPIO_SETUP_SWITCH
}

check_setup_button()
{
        setup_state=$(read_setup_state)
        while [ $setup_state -eq 1 ]
        do
        echo "Setup switch up, waiting setup to be closed"
        sleep 2
        setup_state=$(read_setup_state)
        done
}

is_router_connected()
{
# check gateway IP address
# there are two OR condition, one from route command and one from "gateway" nvram
        route_ip_address=`route | grep default | cut -d' ' -f10`
        if [ "$route_ip_address" = "" ]; then
                echo "router not connected, waiting connection"
                return 0
        else
                return 1
        fi
}

check_router_connection()
{
        is_router_connected
        rect=$?
        while [ $rect -eq 0 ]
        do
        sleep 2
        is_router_connected
        rect=$?
        done
}

set_gate_ip_address()
{
	echo "netstat -rn says:"
	netstat -nr
	gate_ip_address=`netstat -nr | grep UG | cut -d' ' -f10`
	printf "%s: gate_ip_address now set to \"%s\"\n" "$(date)" "${gate_ip_address}"
	if [ ! -n $gate_ip_address ]; then
        	echo "There is NOT gate information at all"
	else
        	update_nvram_value "MyGateIP" "$gate_ip_address"
	fi
}

check_ntp_update()
{
        ntp_from_netcam=`nvram_get 2860 NetCamTimeZoneFlag`
        NOW_YEAR=`date +"%Y"`
        #if [ $ntp_from_netcam -eq 1 ]; then
                echo "time sync from NetCam, to check update status"
                while [ $NOW_YEAR -lt 2013 ]
                do
                echo "waiting for NTP time sync"
                sleep 10
                NOW_YEAR=`date +"%Y"`
                done
                echo "NTP UPDATE DONE"
}

# Ensure external requirements are met before we can run.
# Currently, this is only that the SSL certificates are in place.
check_certs() {
    BSETTINGS="/tmp/Belkin_settings/"
    BSRC="/sbin"
    CERT_SRC="/sbin/cacerts"
    CERT_LINK="/etc/certs"
    # Ensure link to certs is present
    if [ ! -e "$CERT_LINK" ]; then
        echo "Creating certificate link"
        ln -s $CERT_SRC $CERT_LINK
    else
        printf "Cert link '%s' already present (%s).\n" "$CERT_LINK" "$(ls -ld $CERT_LINK)"
    fi
}


[ -x /sbin/pmortemd ] && /sbin/pmortemd 

killall natClient
killall wemoApp
sleep 2

check_certs

#it should keep wait here until the setup button OFF
check_setup_button
check_router_connection
sleep 20
get_wlan_ip
check_ntp_update
sleep 5

set_gate_ip_address

update_wan_if
get_wan_ip

cp -f /etc/resolv.conf /tmp/
cp /sbin/ver.txt /etc/ver.txt
cp /sbin/devices.txt /etc/devices.txt
# - this is for the compatile with Gemtek SDK, NetCam SDK limited the copy to /etc
cp /sbin/sensor.jpg /etc/sensor.jpg
cp /sbin/sensor.jpg /tmp/Belkin_settings/icon.jpg

/sbin/natClient &
/sbin/wemoApp -webdir /tmp/Belkin_settings/&
sleep 60

first_timezone=`cat /etc/TZ`
current_timezone=

while true
do
	update_wan_if
	get_wan_ip
        # Check thread number, each 5 seconds
        thread_count=`ps | grep -c wemoApp`
        if [ $thread_count -gt 60 ] || [ $thread_count -lt 20 ]; then
                echo "System thread exception, will killall thread and quit: number $thread_count"
                if [ $thread_count -eq 0 ]; then
		        /sbin/natClient &
                        /sbin/wemoApp -webdir /tmp/Belkin_settings/&
                else
		        killall natClient
                        killall wemoApp
                        sleep 5
			/sbin/natClient &
                        /sbin/wemoApp -webdir /tmp/Belkin_settings/&
                        sleep 60
                fi
        fi
        sleep 5
        current_timezone=`cat /etc/TZ`
        if [ "$current_timezone" != "$first_timezone" ]; then
                echo "#######################Time zone change system to reload#########################################"
                first_timezone=$current_timezone
                sleep 20
		killall natClient
                killall wemoApp
                sleep 5
        fi
        # Check Setup
        setup_state=$(read_setup_state)
        if [ $setup_state -eq 1 ]; then
                # what should do, and goto where?
	        killall natClient
                killall wemoApp
                check_setup_button
                cp -f /etc/resolv.conf /tmp/
                get_wlan_ip
                # set_gate_ip_address
                sleep 20
        fi
done

