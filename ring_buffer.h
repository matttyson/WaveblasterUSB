#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#ifndef RING_BUFFER_LENGTH
	//length has to be hard coded in.
	#error You must define RING_BUFFER_LENGTH before including this file!
#endif

// Must be a power of 2 for the bitwise hackery to work.
#if ((RING_BUFFER_LENGTH & (RING_BUFFER_LENGTH - 1)) != 0)
	#error ring buffer length must be a power of 2
#endif

#include <stdint.h>

struct ring_buffer {
	uint8_t head;
	uint8_t tail;
	uint8_t size;
	uint8_t buffer[RING_BUFFER_LENGTH];
};
typedef struct ring_buffer ring_buffer_t;

// Init the ring buffer
#define RING_INIT(x) (x).head = 0; (x).tail=0; (x).size = 0;

// Return TRUE if the ring buffer is empty
#define RING_EMPTY(x) ((x).size == 0)

// Return TRUE if the ring buffer is not empty
#define RING_NOT_EMPTY(x) (((x).head) != ((x).tail))

// Queue byte 'y' in ring buffer 'x'
#define RING_QUEUE_BYTE(x,y) \
	(x).buffer[((x).head)] = (y); \
	(x).head = (((x).head + 1) & (RING_BUFFER_LENGTH-1)); \
	(x).size++;

// Dequeue byte from ring buffer 'x' into 'y'
#define RING_DEQUEUE(x,y ) \
	(y) = (x).buffer[((x).tail)]; \
	(x).tail = (((x).tail + 1) & (RING_BUFFER_LENGTH-1)); \
	(x).size--;

#define RING_FULL(x) ((((x).head+1) & (RING_BUFFER_LENGTH-1)) == (x).tail)
#define RING_SIZE(x) ((x).size)

#endif