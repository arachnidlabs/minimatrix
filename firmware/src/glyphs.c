#include <inttypes.h>
#include <avr/pgmspace.h> 

#include "glyphs.h"

unsigned const char glyphs[] PROGMEM = {
	0b00000000, // 0 - Play
	0b00000000,
	0b11111110,
	0b01111100,
	0b00111000,
	0b00111000,
	0b00010000,
	0b00000000,
	
	0b00000000, // 1 - Marquee editor
	0b00000000,
	0b01100000,
	0b01000000,
	0b01111110,
	0b01000000,
	0b01100000,
	0b00000000,
};
