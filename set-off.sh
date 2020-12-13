#!/bin/sh
mosquitto_pub -r -t 'devices/led-0002/led_0/on/set' -m '0'
