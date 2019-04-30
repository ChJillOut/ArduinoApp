We wrote read_usb and server in single C file. 

Therefore "gcc -o server server.c" can be used to compile the code, 

and "./server /dev/cu.usbmodem14101 3001" are the arguments that needed to run this code. 