#!/bin/sh

while true; do
        /sbin/wemoApp -webdir /etc/Belkin_settings/ &>/dev/console
        [ "$(nvram get SAVE_MULTIPLE_LOGS)" != "" ] || rm -f /tmp/messages-*
        cp /var/log/messages /tmp/messages-$(date +%Y-%m-%d-%H:%M)
        killall wemoApp
        sleep 2
done
