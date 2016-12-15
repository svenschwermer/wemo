#!/bin/sh

WeMoWaitForzigbeed() {
	COUNTER=0
	while [ $COUNTER -lt 5 ]; do
		sleep 1
		pidof zigbeed > /dev/null
		if [ $? -eq 0 ]; then
			break
		fi
		COUNTER=$(expr $COUNTER + 1)
	done
}

WeMoStartZigBee() {
	killall zigbeed
	sleep 1

	/sbin/zigbeed -d &

	WeMoWaitForzigbeed
}

FLAG_NO_RERUN="/tmp/zigbee.no.rerun"
ZB_NVRAM=ClientSSID

while true
do
	sleep 2
	[ -e ${FLAG_NO_RERUN} ] && continue;

	pidof zigbeed > /dev/null
	if [ $? -ne 0 ]; then
		ZB_SSID=$(nvram_get ${ZB_NVRAM} | cut -f2 -d =)
		[ -z ${ZB_SSID} ] && continue;
		echo "ReStarting zigbeed..."
		WeMoStartZigBee
	fi
done
