#include <xc.h>
#include <stdint.h>

#include "system.h"
#include "HardwareProfile.h"
#include "usb_config.h"


#include "USB/usb.h"
#include "USB/usb_device_midi.h"
#include "USB/usb_device.h"

// Ring buffer needs to be 64 bytes to contain a large sysex packet.
#define RING_BUFFER_LENGTH 64
#include "ring_buffer.h"

static void USBCBInitEP(void);

static void init_state(void);
static void send_reset(void);
static void all_notes_off(void);

#define WRITE_MIDI(x,y) \
	if(TXSTAbits.TRMT && RING_NOT_EMPTY((x))) \
	{ RING_DEQUEUE((x),(y));}
#define BusyUSART() (!TXSTAbits.TRMT)

#define LED_PIN LATCbits.LATC3
#define LED_OFF() { LED_PIN = 0; }
#define LED_ON()  { LED_PIN = 1; }
#define LED_TOGGLE() { LED_PIN = ! LED_PIN; }

#define RESET_PIN LATCbits.LATC5
#define RESET_LOW()  { RESET_PIN = 0; }
#define RESET_HIGH() { RESET_PIN = 1; }

//                          T2CONbits.TMR2ON = 1;
#define LED_TIMER T1CONbits.TMR1ON
#define START_LED_FLASH() { LED_TIMER = 1; }
#define STOP_LED_FLASH()  { LED_TIMER = 0; LED_PIN = 0; }

#define STOP_TX_INT()  { PIE1bits.TXIE = 0; }
#define START_TX_INT() { PIE1bits.TXIE = 1; }

#define ATTACH_USB() { USBDeviceAttach(); }
#define DETACH_USB() { USBDeviceDetach(); all_notes_off(); }

// 920 is the remapped address where the bootloader expects this data.
const static unsigned char VERSION[2] = /*@0x920*/ {
	APP_VERSION_MAJOR, APP_VERSION_MINOR
};


union midi_buffer {
	USB_AUDIO_MIDI_EVENT_PACKET packets[16];
	uint8_t buffer[64];
};
typedef union midi_buffer midi_buffer_u;

// This is where data will be buffered while waiting for serial TX
volatile ring_buffer_t ring_buffer @0x0320;
// This is the buffer for the data coming in from USB.
midi_buffer_u rx_buffer   @0x02A0;


USB_HANDLE USBRxHandle = 0;

// Midi packet length, http://www.usb.org/developers/devclass_docs/midi10.pdf page 16
const static uint8_t cin_packet_length[] = {0,0,2,3,3,1,2,3,3,3,3,3,2,2,3,1};

void main(void)
{
	uint8_t i;
	uint8_t j;
	uint8_t rx_len;
	uint8_t cin_len;

	init_state();
	send_reset();

	RING_INIT(ring_buffer);

	USBDeviceInit();

	// Assume USB is always attached.
	#ifdef USB_INTERRUPT
		ATTACH_USB();
	#endif
	while(1) {

		#ifdef USB_POLLING
		USBDeviceTasks();
		#endif

		if((USBDeviceState < CONFIGURED_STATE) || (USBSuspendControl == 1)){
			continue;
		}

		if(!USBHandleBusy(USBRxHandle)){
			// Find out how much data we have received
			rx_len = USBHandleGetLength(USBRxHandle);

			// We need at least 48 bytes free in the buffer before we accept the
			// next data packet.  a full buffer will cause an overflow if we accept
			// it too early.
			while((rx_len + RING_SIZE(ring_buffer)) > RING_BUFFER_LENGTH){
				// We don't have enough free space in the buffer to accept this
				// packet. keep spinning until we do.
				#ifndef TX_INTERRUPT
				WRITE_MIDI(ring_buffer,TXREG)
				#endif
				#ifdef USB_POLLING
				USBDeviceTasks();
				#endif
			}

			// XC8 Free mode is daft and actually does a divide.
			// force a shift instead.
			rx_len = rx_len >> 2;

			for(i = 0; i < rx_len; i++){
				cin_len = cin_packet_length[rx_buffer.packets[i].CodeIndexNumber];
				for(j = 1; j <= cin_len; j++){
					RING_QUEUE_BYTE(ring_buffer, rx_buffer.packets[i].v[j]);
				}
				#ifdef TX_INTERRUPT
					// We've queued up some data, enable the transmit ready interrupt.
					START_TX_INT();
				#else
					// Begin writing the midi data out.
					WRITE_MIDI(ring_buffer,TXREG)
				#endif
			}
			USBRxHandle = USBRxOnePacket(MIDI_EP,(uint8_t*)&(rx_buffer),sizeof(rx_buffer));
		}
		#ifndef TX_INTERRUPT
		WRITE_MIDI(ring_buffer,TXREG)
		#endif
	}
}

static void
init_state(void)
{
#ifdef INTERNAL_OSC
	// Oscillator self tune for USB.
	ACTCON = 0x90;

	// PLL On, x3 multiplier
	// 16mhz internal osc
	OSCCON = 0xFC;

	// Wait for the oscillator to stabilize
	while(OSCSTATbits.PLLRDY == 0){
		__nop();
	}
#endif

	// All pins digital.
	ANSELA = 0x00;

	// Set pins output.
	TRISA = 0x00;
	// set RC2 input (reset switch)
	TRISC = 0x04;

	// All pins low.
	LATA = 0x00;
	LATC = 0x00;

#ifdef TX_INTERRUPT
	INTCONbits.GIE	= 1; // Global interrupt enable
	INTCONbits.PEIE	= 1; // Serial TX Complete interrupt
#endif

	// Baud rate of 31250 with a 48mhz clock, 6x clock divider
	SPBRG = ((48000000/6)/64/31250)-1;

	// Enable UART. 8N1
	RCSTAbits.SPEN = 1;
	TXSTAbits.TX9  = 0;
	TXSTAbits.SYNC = 0;
	TXSTAbits.TXEN = 1;

	// Set up Timer 2 for the LED flash.
	//PIE1bits.TMR2IE = 1;
	//T2CON = 0xFB;

	T1CON = 0x30;
//	T1CONbits.TMR1ON = 1;
	PIE1bits.TMR1IE = 1;
	INTCONbits.PEIE = 1;
}

/**
 * This sends a reset signal to the waveblaster.  It's required at power on.
 */
static void
send_reset(void)
{
	RESET_HIGH()

	__delay_ms(250);

	RESET_LOW()
}

/* Tell the synth to shut the hell up */
static void
all_notes_off(void)
{
	uint8_t i;

	STOP_TX_INT();

	// empty out the ring buffer
	RING_INIT(ring_buffer);

	for(i = 0; i < 0x10; i++) {
		RING_QUEUE_BYTE(ring_buffer, (0xB0 | i));
		RING_QUEUE_BYTE(ring_buffer, 0x78);
		RING_QUEUE_BYTE(ring_buffer, 0)
	}

	START_TX_INT();
}

uint8_t
USER_USB_CALLBACK_EVENT_HANDLER (USB_EVENT event, void *pdata, uint16_t size)
{
	switch(event){
		case EVENT_CONFIGURED:
			USBCBInitEP();
			break;
	}
	return 1;
}

static void
USBCBInitEP(void)
{
	//enable the HID endpoint
	USBEnableEndpoint(MIDI_EP,USB_OUT_ENABLED|USB_IN_ENABLED|USB_HANDSHAKE_ENABLED|USB_DISALLOW_SETUP);

	//Re-arm the OUT endpoint for the next packet
	USBRxHandle = USBRxOnePacket(MIDI_EP,(uint8_t*)&(rx_buffer),sizeof(rx_buffer));
}


void interrupt
interrupt_routine(void)
{
	// USB Device interrupt.
	#ifdef USB_INTERRUPT
	if(USBInterruptFlag){
		USBDeviceTasks();
		USBClearUSBInterrupt();
	}
	#endif


	// Serial TX Complete interrupt.
	#if defined(TX_INTERRUPT)
	if(PIE1bits.TXIE && PIR1bits.TXIF){
		// Send the next byte out the serial port.
		RING_DEQUEUE(ring_buffer, TXREG);

		if(RING_EMPTY(ring_buffer)){
			STOP_TX_INT(); // No more data, shut down interrupt.
		}
	}
	#endif

	// Toggle the LED
	if(PIR1bits.TMR1IF) {
		LED_TOGGLE();
		PIR1bits.TMR1IF = 0;
	}
}
