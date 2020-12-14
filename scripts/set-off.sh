#!/bin/sh
for i in 0 1 2
do
	mosquitto_pub -r -t devices/led-0002/led_${i}/on/set -m '0'
done
