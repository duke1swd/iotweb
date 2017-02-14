#!/bin/sh
mosquitto_pub -t 'devices/led0001/$reset' -m 'true'
