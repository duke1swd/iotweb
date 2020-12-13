#!/bin/sh
mosquitto_pub -r -t 'devices/plug-0002/outlet/on/set' -m 'true'
sleep 1
mosquitto_pub -r -t 'devices/plug-0002/outlet/on/set' -m 'false'
mosquitto_pub -r -t 'devices/plug-0002/button/button/set' -m 'false'
