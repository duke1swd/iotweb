#!/bin/sh
mosquitto_pub -r -t 'devices/led0001/local-led/on/set' -m 'true'
