# cmojoloader

a loader for the [embeddedmicro](https://embeddedmicro.com) mojo v3 fpga development board

when plugged in via usb it presents itself as an acm device (eg. `/dev/ttyACM0`)

this interface can be used to load a bitfile generated via the xilinx ise toolchain

## command interface 
~~~~
erase flash
write byte 'E', read byte 'D' if OK.

write to flash with verification
write byte 'V', read byte 'R' if OK.

write to flash without verification
write byte 'F', read byte 'R' if OK.

write to ram
write byte 'R', read byte 'R' if OK.

write size of bitfile packed as little endian.
read byte 'O' if OK.

write bitfile stream
read byte 'D' if OK.

write byte 'S' to get size
read byte 0xAA if OK, next four bytes in little endian specify flash size.
read rest of bitstream to validate.

load from flash
write byte 'L', read byte 'D' if OK.
~~~~

## latest firwmware
this loader requires the latest firmware be present on the device.
to update the firmware, short the `gnd` and `rst` pins on the back of the board during power up.

the device should present itself as atmel device in `dmesg` or `lsusb`, then run:

`dfu-programmer atmega32u4 flash Mojo-vX-Loader-X.X.X.hex`

the latest firmware can be obtained from the [embeddedmicro website](https://embeddedmicro.com/tutorials/mojo-software-and-updates/updating-the-mojo)

