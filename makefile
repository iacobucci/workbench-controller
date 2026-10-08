.PHONY: compile upload monitor clean

PORT ?= /dev/ttyACM0
BAUD ?= 115200

all:
	make compile
	make run

compile:
	pio run

upload:
	pio run -t upload --upload-port $(PORT)

monitor:
	pio device monitor -p $(PORT) -b $(BAUD)

clean:
	pio run -t clean
