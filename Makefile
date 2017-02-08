.PHONY:

all:	main.c s5p_boot.h s5p_usb.h
	gcc -Wall -Wextra -Os -I/usr/local/include/libusb-1.0 main.c -L/usr/local/lib -lusb-1.0 -o load

