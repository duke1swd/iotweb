/home/duke1swd/.platformio/penv/bin/python3 \
	/home/duke1swd/.platformio/packages/tool-esptoolpy/esptool.py \
		--before default_reset \
		--after hard_reset \
		--chip esp8266 \
		--port "/dev/ttyUSB0" \
		--baud 115200 \
		write_flash 0x0 blank_1MB.bin
