#!/bin/sh
if [ x$1 = x ] ; then
	echo Usage $0 '<state>' 1>&2
	exit 1
fi
echo Got state $1
mosquitto_pub -r -t 'devices/alarm-state-0001/alarm-state/state' -m $1
