#!/bin/bash
function on_exit {
	rm -f /tmp/config.json
}

trap on_exit exit

# Insert the password into config.json
PASS=`tail -1 /usr/local/credentials/DanielIOT.txt`
sed < config.json > /tmp/config.json -e s/PASSWORD/$PASS/

# Config the device!
curl -X PUT http://192.168.123.1/config --header "Content-Type: application/json" -d @/tmp/config.json
