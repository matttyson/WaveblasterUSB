#ifndef HARDWARE_PROFILE_H
#define HARDWARE_PROFILE_H

#include <xc.h>

/*
 Defines we have
 * USB_INTERRUPT  - Handle USB servicing via interrupt routine
 * TX_INTERRUPT   - Handle serial transmission via interrupt routine
 * USE_BOOTLOADER - do the magic required for operation with a bootloader.
 * INTERNAL_OSC   - Use the internal oscillator, instead of an external crystal.
 */

/* Firmware version */
#define APP_VERSION_MAJOR 0
#define APP_VERSION_MINOR 1

#endif