#
# Makefile for Phoenix USB device.
#
all: phoenix-usb

clean:
	rm -f phoenix-usb *.o *.a

phoenix-usb: phoenix-usb.c
	gcc -o phoenix-usb phoenix-usb.c
