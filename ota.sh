#!/bin/sh
mosquitto_pub -t 'devices/led0001/$ota' -m '0.2.2'
