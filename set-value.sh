#!/bin/sh
mosquitto_pub -r -t 'devices/led-0001/led/on/set' -m '5'
