#!/bin/sh
mosquitto_pub -r -t 'devices/led-0002/led_2/on/set' -m '10'
mosquitto_pub -r -t 'devices/led-0002/led_2/intensity/set' -m '16'
