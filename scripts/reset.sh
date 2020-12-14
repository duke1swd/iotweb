#!/bin/sh
mosquitto_pub -t 'devices/environ-0001/$implementation/reset' -m 'true'
