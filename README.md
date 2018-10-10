# WaveblasterUSB

This programs a PIC16F1455 to act as a USB to MIDI device.

The code is really quite simple.  It reads the USB packet as it comes in and then writes the packet out to the serial port.

It makes use of the Microchip USB stack to do the USB heavy lifting

## Building

To build this you will need Microchip's MPLABX IDE and XC8 C compiler.

## What is a Waveblaster?

The Waveblaster was a hardware MIDI synthesizer that clipped on to a pin header on the side of sound cards such as the Sound Blaster 16, AWE32 and other sound cards that were common in the mid 90s.

Roland made the SCB-55, Yamaha had the DB50XG.  Other manufacturers made their own versions.

This is part of a larger circuit that allows any waveblaster type synthesizer to be hooked up to a modern PC via USB for dosbox retrogaming.
