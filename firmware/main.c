#define F_CPU 8000000UL  // 8 MHz

#include <inttypes.h>
#include <avr/eeprom.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <stdio.h>

#include "glcdfont.h"

FUSES = {
	// Internal 8mhz oscillator; short startup; enable ISP programming.
	.low = FUSE_SUT0 & FUSE_CKSEL3 & FUSE_CKSEL1 & FUSE_CKSEL0,
	.high = FUSE_SPIEN,
};

#define PWM_ON() (TCCR1B |= _BV(CS11))
#define PWM_OFF() (TCCR1B &= ~_BV(CS11))

#define MAX_MESSAGE_LENGTH 248

#define IR_MESSAGE_LENGTH 14

#define MODE_MARQUEE 0
#define MODE_EDIT 1

typedef union {
	uint16_t data;
	struct {
		unsigned int command : 11;
		unsigned int toggle : 1;
		unsigned int field : 1;
		unsigned int empty : 3;
	} message;
} ir_message_t;

typedef struct {
	uint8_t delay;
	uint8_t spacing;
} config_t;

typedef void (*mode_func)(void);

config_t stored_config EEMEM = { .delay = 15, .spacing = 1 };
char message[MAX_MESSAGE_LENGTH] EEMEM = "www.arachnidlabs.com/minimatrix/ ";

config_t config;
uint8_t display[8] = {0, 0, 0, 0, 0, 0, 0, 0};
uint8_t row = 0;
uint8_t last_ir_level = 1;
uint8_t ir_counter = 0;
uint8_t ir_bit_counter = 0;
ir_message_t current_message;
mode_func mode;
 
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

#define PORTD_ROWS _BV(PD6) | _BV(PD5) | _BV(PD4) | _BV(PD3) | _BV(PD2) | _BV(PD1)
#define PORTA_ROWS _BV(PA1) | _BV(PA0)

#define ENABLE_ROW(row) if(row < 6) PORTD &= ~row_pins[row]; else PORTA &= ~row_pins[row]

#define COMMAND_NONE 	0xFFFF
#define COMMAND_STANDBY 0x000C
#define COMMAND_UP		0x0020
#define COMMAND_DOWN	0x0021
#define COMMAND_LEFT	0x0011
#define COMMAND_RIGHT	0x0010
#define COMMAND_MENU	0x000B

int8_t previous_toggle = -1;

uint16_t get_message() {
	if(ir_bit_counter < IR_MESSAGE_LENGTH)
		return COMMAND_NONE;
	return current_message.message.command;
}

uint8_t message_is_repeat() {
	return previous_toggle == current_message.message.toggle;
}

void ir_clear_buffer(void) {
	previous_toggle = current_message.message.toggle;
	ir_bit_counter = 0;
}

void ir_receive(void) {
	if(ir_bit_counter == IR_MESSAGE_LENGTH)
		return;

	uint8_t ir_level = PIND & _BV(PD0);
	if(ir_level == last_ir_level) {
		// Increment the time intervals since last bit flip
		ir_counter++;
		if(ir_counter > 20)
			ir_clear_buffer(); // Long pause, reset
	} else {
		if(ir_counter > 8) {
			// Direction of transition indicates bit value
			// space to mark (ir_level=0) is a 1, mark to space is 0.
			current_message.data <<= 1;
			if(!ir_level)
				current_message.data |= 1;
			ir_bit_counter++;
			ir_counter = 0;
		} else {
			// This transition is between bits, ignore it
		}
		
		last_ir_level = ir_level;
	}
}

ISR(TIMER1_COMPA_vect) {
	// Turn off the old row
	PORTD |= PORTD_ROWS;
	PORTA |= PORTA_ROWS;
	
	// Poll the IR receiver
	ir_receive();

	// Set the column data
	row = (row + 1) & 0x7;
	PORTB = display[row];
	
	// Turn on the new row
	ENABLE_ROW(row);
}

void ioinit(void) {
	OCR1A = 125; // 8 megahertz / 8 / 125 = 8KHz
	TIMSK |= _BV(OCIE1A); // Interrupt on counter reset
	TCCR1B = _BV(WGM12); // CTC(OCR1A), /8 prescaler

	// PORTB is all output for columns
	DDRB = 0xff;

	// PORTD is output for rows except PD0 which is the IR input
	DDRD = PORTD_ROWS;
	// Enable pullup on IR receiver pin
	PORTD |= _BV(PD0);

	// PORTA is output for rows
	DDRA = PORTA_ROWS;
	
	sei();
}

uint8_t read_font_column(uint8_t character, uint8_t column) {
	return pgm_read_byte(font + (character * 5) + column);
}

void marquee(void);
void edit(void);

void marquee(void) {
	char *msgptr = message;
	while(mode == marquee) {
		uint8_t current = eeprom_read_byte((uint8_t*)msgptr++);
		if(current == '\0') { 
			msgptr = message;
			continue;
		}

		int j, k;
		for(j = 0; j < 5 + config.spacing; j++) {
			// Shift everything else left
			for(k = 7; k > 0; k--)
				display[k] = display[k - 1];
			
			if(j >= 5) {
				// Empty columns at the end
				display[0] = 0;
			} else {
				// Display the next column of the current letter 
				display[0] = read_font_column(current, j);
			}

			for(k = 0; k < config.delay && mode == marquee; k++) {
				_delay_ms(10);
				uint16_t cmd = get_message();
				if(cmd == COMMAND_NONE) continue;
				
				switch(cmd) {
				case COMMAND_MENU:
					if(!message_is_repeat())
						mode = edit;
					break;
				}
				ir_clear_buffer();
			}
		}
	}
}

void edit() {
	uint8_t repeats = 0, idx = 0;
	char current;
	

	void write_dirty(void) {
		eeprom_update_byte((uint8_t*)&message[idx], current);
	}
	
	void read_current(void) {
		current = eeprom_read_byte((uint8_t*)&message[idx]);
	}
	
	display[0] = 0;
	display[6] = 0;
	display[7] = 0;
	
	// Load and show the first character
	read_current();
	for(int i = 0; i < 5; i++)
		display[5 - i] = read_font_column(current, i);

	while(mode == edit) {
		uint16_t cmd = get_message();
		if(cmd == COMMAND_NONE) {
			repeats = 0;
			continue;
		}

		switch(get_message()) {
		case COMMAND_LEFT:
			if(idx > 0) {
				write_dirty();
				idx--;
				read_current();
				for(int i = 0; i < 8; i++) {
					for(int j = 0; j < 7; j++)
						display[j] = display[j + 1];
					if(i < 5) {
						display[7] = read_font_column(current, i);
					} else {
						display[7] = 0;
					}
					_delay_ms(100);
				}
			}
			break;
		case COMMAND_RIGHT:
			if(idx < MAX_MESSAGE_LENGTH && current != '\0') {
				write_dirty();
				idx++;
				read_current();
				for(int i = 0; i < 8; i++) {
					for(int j = 7; j > 0; j++)
						display[j] = display[j - 1];
					if(i < 5) {
						display[0] = read_font_column(current, 4 - i);
					} else {
						display[0] = 0;
					}
					_delay_ms(100);
				}				
			}
			break;
		case COMMAND_UP:
			current++;
			show_current();
			break;
		case COMMAND_DOWN:
			current--;
			show_current();	
			break;
		case COMMAND_MENU:
			if(!message_is_repeat()) {
				write_dirty();
				mode = marquee;
			}
			break;
		default:
			continue;
		}

		if(repeats == 0)
			_delay_ms(750);
		ir_clear_buffer();
		_delay_ms(250);
		repeats++;
	}
}

int main(void) {
	eeprom_read_block(&config, &stored_config, sizeof(config_t));
	
	mode = marquee;
	
	ioinit();

	PWM_ON();
	
	for(;;) {
		mode();
	}
}
