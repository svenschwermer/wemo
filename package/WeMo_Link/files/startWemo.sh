#!/bin/sh

while true; do
    /sbin/natClient &>/dev/console &
    /sbin/wemoApp -webdir /tmp/Belkin_settings/ &>/dev/console
    killall natClient
    killall wemoApp
    if [ "$(nvram get SAVE_MULTIPLE_LOGS)" = "{ NULL String }" -o "$(nvram get SAVE_MULTIPLE_LOGS)" = "" ]; then
	    rm -f /tmp/messages-*
    fi
    cp /var/log/messages /tmp/messages-$(date +%Y-%m-%d-%H:%M)
    sleep 2
done
