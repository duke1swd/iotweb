#!/bin/sh
mosquitto_pub -r -t 'devices/led-0002/led_1/on/set' -m '10'
mosquitto_pub -r -t 'devices/led-0002/led_2/on/set' -m '10'
while true; do
	for i in 1 2 ; do
		mosquitto_pub -r -t devices/led-0002/led_${i}/intensity/set -m '8'
	done
	sleep 1
	for i in 1 2 ; do
		mosquitto_pub -r -t devices/led-0002/led_${i}/intensity/set -m '128'
	done
	sleep 1
	for i in 1 2 ; do
		mosquitto_pub -r -t devices/led-0002/led_${i}/intensity/set -m '255'
	done
	sleep 1
done
