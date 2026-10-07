all:
	make compile
	make upload
	make monitor

compile:
	arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi ./hue-switch.ino

upload:
	arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:renesas_uno:unor4wifi ./hue-switch.ino

monitor:
	arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200