#!/bin/sh
mosquitto_pub -r -t 'devices/plug-0001/outlet/on/set' -m 'true'
sleep 1
mosquitto_pub -r -t 'devices/plug-0001/outlet/on/set' -m 'false'
