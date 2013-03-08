#define F_CPU 8000000UL  // 8 MHz

#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <util/delay.h>

FUSES = {
	.low = FUSE_SUT0 & FUSE_CKSEL3 & FUSE_CKSEL1 & FUSE_CKSEL0,
	.high = FUSE_SPIEN,
};

char display[8] = {0, 0, 0, 0, 0, 0, 0, 0};
int row = 0;

const char row_pins[] = {
	_BV(PD6),
	_BV(PD5),
	_BV(PD4),
	_BV(PD3),
	_BV(PD2),
	_BV(PD1),
	_BV(PD0),
	_BV(PA0)
};

ISR(TIMER1_COMPA_vect) {
	// Turn off the old row
	PORTD = 0xff;
	PORTA = 0xff;
	
	// Set the column data
	row = (row + 1) & 0x7;
	PORTB = display[row];
	
	// Turn on the new row
	if(row < 7) {
		PORTD &= ~row_pins[row];
	} else {
		PORTA &= ~row_pins[row];
	}
}

void ioinit(void) {
	OCR1A = 1250; // 8 megahertz / 8 / 1250 = 800 hertz
	TIMSK |= _BV(OCIE1A); // Interrupt on counter reset
	TCCR1B = _BV(WGM12) | _BV(CS11); // CTC(OCR1A), /8 prescaler
	
	DDRB = 0xff;
	DDRD = _BV(PD1) | _BV(PD2) | _BV(PD3) | _BV(PD4) | _BV(PD5) | _BV(PD6);
	DDRA = _BV(PA0) | _BV(PA1);
	
	sei();
}

int main(void) {
	ioinit();
	
	int v = 1;
	for(;;) {
		for(int r = 0; r < 8; r++) {
			display[r] ^= 0xff;
			_delay_ms(250);
			/*for(int c = 0; c < 8; c++) {
				display[r] <<= 1;
				_delay_ms(250);
			}*/
		}
		//v = (v << 2) | 1;
	}
}

