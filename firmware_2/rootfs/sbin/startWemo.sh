#!/bin/sh

val=`df | grep "mini_fo:/overlay" | awk '{print $1}'`
counter=0
while [ "$val" != "mini_fo:/overlay" ] && [ "$counter" -lt 30 ]
do
	sleep 1
	counter=`expr $counter + 1`
	val=`df | grep "mini_fo:/overlay" | awk '{print $1}'`
done

killall -9 psmon wan_connect ledctrl udhcpc
# mount sysfs needed for hotplug subsystem
mount none -t sysfs /sys

while true; do
    /sbin/natClient &>/dev/console &
    /sbin/wemoApp -webdir /tmp/Belkin_settings/ &>/dev/console
    if [ -e /tmp/rebooting ]; then
	exit
    fi
    killall natClient
    killall wemoApp
    if [ "$(nvram get SAVE_MULTIPLE_LOGS)" = "{ NULL String }" -o "$(nvram get SAVE_MULTIPLE_LOGS)" = "" ]; then
	    rm -f /tmp/messages-*
    fi
    cp /var/log/messages /tmp/messages-$(date +%Y-%m-%d-%H:%M)
    sleep 2
done
