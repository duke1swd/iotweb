#!/bin/sh
mosquitto_pub -r -t 'devices/alarm-state-0001/led/on/set' -m 'true'
