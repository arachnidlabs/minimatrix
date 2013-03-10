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

#define PWM_ON() (TCCR1B |= _BV(CS11))
#define PWM_OFF() (TCCR1B &= ~_BV(CS11))

char display[8] = {0, 0, 0, 0, 0, 0, 0, 0};
int row = 0;

const char row_pins[] = {
	_BV(PD6),
	_BV(PD5),
	_BV(PD4),
	_BV(PD3),
	_BV(PD2),
	_BV(PD1),
	_BV(PA1),
	_BV(PA0)
};

#define ENABLE_ROW(row) if(row < 6) PORTD &= ~row_pins[row]; else PORTA &= ~row_pins[row]

ISR(TIMER1_COMPA_vect) {
	// Turn off the old row
	PORTD = 0xff;
	PORTA = 0xff;
	
	// Set the column data
	row = (row + 1) & 0x7;
	PORTB = display[row];
	
	// Turn on the new row
	ENABLE_ROW(row);
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

void sense_columns() {
	PWM_OFF();

	// Make the columns output low
	PORTB = 0;

	// Bring all the rows high
	PORTD = 0xff;
	PORTA = 0xff;
	
	_delay_us(100);

	// Now make the columns inputs
	DDRB = 0;
		
	_delay_ms(6);
	
	// Now read the columns
	int i;
	for(i = 7; i >= 1; i--)
		display[i] = display[i-1];
	display[0] = ~PINB;

	// Make the columns outputs again
	DDRB = 0xff;

	PWM_ON();
}

int main(void) {
	ioinit();
	
	for(;;) {
		sense_columns();
		_delay_ms(1000);
	}
}

