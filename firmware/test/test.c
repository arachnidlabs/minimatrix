#define F_CPU 8000000UL  // 8 MHz

#include <inttypes.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "../src/hardware.h"

static void ioinit(void) {
	// Set rows and columns as output
	COLUMN_DDR = 0xff;
	DDRD |= PORTD_ROWS;
	DDRA |= PORTA_ROWS;
//	PORTB = 0x255;
//	PORTD |= PORTD_ROWS;
//	PORTA |= PORTA_ROWS;

	sei();
}

struct {
	volatile uint8_t *port;
	uint8_t pin;
} out_pins[] = {
	{&PORTD, _BV(PD1)},
	{&PORTA, _BV(PA0)},
	{&PORTB, _BV(PB1)},
	{&PORTB, _BV(PB2)},
	{&PORTA, _BV(PA1)},
	{&PORTB, _BV(PB4)},
	{&PORTD, _BV(PD0)},
	{&PORTD, _BV(PD4)},
	{&PORTD, _BV(PD6)},
	{&PORTB, _BV(PB3)},
	{&PORTB, _BV(PB5)},
	{&PORTD, _BV(PD3)},
	{&PORTB, _BV(PB0)},
	{&PORTD, _BV(PD5)},
	{&PORTB, _BV(PB6)},
	{&PORTB, _BV(PB7)},
};

static uint8_t num_pins = 16;

void main(void) __attribute__((noreturn));

void main(void) {
	ioinit();

	int idx = 6;
	while(1) {
		// Wait for rising edge on IR PIN
		while(!(PIND & _BV(IR_PIN)));

		*out_pins[idx].port &= ~out_pins[idx].pin;
		idx = (idx + 1) % num_pins;
		*out_pins[idx].port |= out_pins[idx].pin;

		// Wait for falling edge on IR PIN
		while(PIND & _BV(IR_PIN));
	}
}
