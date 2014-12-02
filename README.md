RFID
====

a simple lib for RC522 Rfid compatible with Arduino's 32bit and Teensy3

This was originally created by Dr. Leong and modified by Miguel Balboa.<br>
This version was modified to work with 32 bit micros like DUE and Teensy3.x, it's really simple and actually just recognize
all my cards and tags serial, I don't know if works with all cards strain. It's really basic but works and good starting
place since the current Balboa library and the old Leong one not working at all with 32 bit micros and the Balboa one it's pretty heavy for my needs!
I will add basic features to write cards and tags as well a small routìne that recognize the various card but my plan it's maintain small.
It's also SPI transaction compatible so it will work with Arduino 1.5.8 IDE and Teensy IDE.
