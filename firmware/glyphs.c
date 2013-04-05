#include <inttypes.h>
#include <avr/pgmspace.h> 

#include "glyphs.h"

uint8_t glyphs[] PROGMEM = {
	0b00000000, // 0 - Play
	0b00000000,
	0b01111111,
	0b00111110,
	0b00011100,
	0b00011100,
	0b00001000,
	0b00000000,
	
	0b00000000, // 1 - Marquee editor
	0b00000000,
	0b00000110,
	0b00000010,
	0b01111110,
	0b00000010,
	0b00000110,
	0b00000000,
};
