#!/bin/sh
mosquitto_pub -t 'homie/382b780adf70/$implementation/config/set' -f config.json
