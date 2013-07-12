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
#include "glyphs.h"
#include "hardware.h"

FUSES = {
	// Internal 8mhz oscillator; short startup; enable ISP programming.
	.low = FUSE_SUT0 & FUSE_CKSEL3 & FUSE_CKSEL1 & FUSE_CKSEL0,
	.high = FUSE_SPIEN,
};

#define PWM_ON() (TCCR0B |= _BV(CS01) | _BV(CS00), DDRD |= PORTD_ROWS, DDRA |= PORTA_ROWS)
#define ENABLE_ROW(row) if(row < 6) PORTD &= ~pgm_read_byte(&row_pins[row]); else PORTA &= ~pgm_read_byte(&row_pins[row])

#define MAX_DATA_LENGTH 248 // bytes
#define IR_MESSAGE_LENGTH 14 // bits

#define IS_MARQUEE 0
#define IS_ANIMATION 1

#define COMMAND_NONE 		0xFFFF
#define COMMAND_STANDBY 	0x04DF
#define COMMAND_UP			0x04D3
#define COMMAND_DOWN		0x04D2
#define COMMAND_LEFT		0x04C1
#define COMMAND_RIGHT		0x04C2
#define COMMAND_MENU		0x04DC
#define COMMAND_ENTER		0x04D1
#define COMMAND_PLAY_PAUSE 	0x04DD

#define EXT_COMMAND_MASK 				0x0700
#define EXT_COMMAND_DATA_ADDR 			0x0700
#define EXT_COMMAND_DATA_WRITE 			0x0600
#define EXT_COMMAND_DISP_BEGIN_WRITE	0x0500
#define EXT_COMMAND_DISP_WRITE 			0x0400
#define EXT_COMMAND_SET_STATE 			0x0300

#define KEY_STANDBY 	0x01
#define KEY_UP 			0x02
#define KEY_DOWN 		0x04
#define KEY_LEFT 		0x08
#define KEY_RIGHT 		0x10
#define KEY_MENU		0x20
#define KEY_ENTER		0x40
#define KEY_PLAY_PAUSE	0x80

#define STATE_NORMAL 0x01
#define STATE_MENU 0x02
#define STATE_SLEEPY 0x04
#define STATE_SLEEPING 0x08
#define STATE_SLAVE 0x10

typedef union {
	uint16_t data;
	struct {
		unsigned int command : 11;
		unsigned int toggle : 1;
		unsigned int field : 1;
		unsigned int empty : 3;
	};
} ir_message_t;

typedef struct {
	uint8_t flags;
	union {
		struct {
			uint8_t delay;
			uint8_t spacing;
		} marquee;
		struct {
			uint8_t delay;
			uint8_t framecount;
		} animate;
	} mode;
} config_t;

typedef struct {
	config_t config;
	uint8_t data[MAX_DATA_LENGTH];
} eedata_t;

typedef void (*mode_func)(void);

typedef struct {
	mode_func run;
	uint8_t glyph_id;
} mode_t;

eedata_t stored_config EEMEM = {
	.config = {
		.flags = IS_MARQUEE,
		.mode = {
			.marquee = {
				.delay = 10,
				.spacing = 1
			}
		}
	},
	.data = "www.arachnidlabs.com/minimatrix/ "
};

static config_t config;
static uint8_t display[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t keypresses = 0;
static uint8_t mode_id = 0;
static volatile uint8_t state = STATE_NORMAL;
static uint8_t config_address = 0; // Address into config buffer for IR writes
static uint8_t display_address = 0; // Address into display buffer for IR writes

const char row_pins[] PROGMEM = {
	_BV(PD6),
	_BV(PD5),
	_BV(PD4),
	_BV(PD3),
	_BV(PD1),
	_BV(PD0),
	_BV(PA0),
	_BV(PA1)
};

inline static void enter_sleep(void) {
	MCUCR &= ~(_BV(ISC01) | _BV(ISC00));
	GIMSK |= _BV(INT0);
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sleep_mode();
	_delay_ms(10);
	MCUCR |= _BV(ISC01);
}

ISR(INT0_vect) { }

inline static void handle_message(ir_message_t *message, uint8_t is_repeat) {
	if((message->command == COMMAND_STANDBY) && !is_repeat) {
		if(state & (STATE_SLEEPING | STATE_SLEEPY)) {
			state = STATE_NORMAL;
			PWM_ON();
		} else {
			state = STATE_SLEEPING;

			// Disable the display
			TCCR0B &= ~(_BV(CS01) | _BV(CS00));
			
			// Set the rows as inputs, with pullups disabled
			DDRD &= ~PORTD_ROWS;
			PORTD &= ~PORTD_ROWS;
			DDRA &= ~PORTA_ROWS;
			PORTA &= ~PORTA_ROWS;
			COLUMN_PORT = 0;
		}
		return;
	}

	if(state & (STATE_SLEEPING | STATE_SLEEPY)) return;

	if(message->field == 1) {
		// Handle regular commands
		switch(message->command) {
		case COMMAND_UP:
			keypresses |= KEY_UP;
			break;
		case COMMAND_DOWN:
			keypresses |= KEY_DOWN;
			break;
		case COMMAND_LEFT:
			keypresses |= KEY_LEFT;
			break;
		case COMMAND_RIGHT:
			keypresses |= KEY_RIGHT;
			break;
		case COMMAND_ENTER:
			keypresses |= KEY_ENTER;
			break;
		case COMMAND_PLAY_PAUSE:
			keypresses |= KEY_PLAY_PAUSE;
			break;
		case COMMAND_MENU:
			if(is_repeat) break;
			state = STATE_MENU;
			break;
		}
	} else {
		if(is_repeat) return;

		// Handle extended commands
		switch(message->command & EXT_COMMAND_MASK) {
		case EXT_COMMAND_DATA_ADDR:
			config_address = message->command & 0xFF;
			break;
		case EXT_COMMAND_DATA_WRITE:
			eeprom_update_byte((uint8_t *)(&config + config_address), message->command & 0xFF);
			config_address++;
			break;
		case EXT_COMMAND_DISP_BEGIN_WRITE:
			display_address = 0;
			// Deliberate fallthrough
		case EXT_COMMAND_DISP_WRITE:
			display[display_address & 0x7] = message->command & 0xFF;
			display_address++;
			break;
		case EXT_COMMAND_SET_STATE:
			state = message->command & 0xFF;
			break;
		}
	}
}

ISR(TIMER1_COMPA_vect) {
	static ir_message_t current_message;
	static ir_message_t previous_message;
	static uint8_t ir_bit_counter = 0;
	static uint8_t last_ir_level = _BV(IR_PIN);
	static uint16_t ir_counter = 127;
	static int16_t repeat_countdown = 0;
	static uint8_t pressed = 0;
	
	if(repeat_countdown > 0)
		repeat_countdown--;

	uint8_t ir_level = PIND & _BV(IR_PIN);
	
	if(ir_level == last_ir_level) {
		// Increment the time interval since last bit flip
		ir_counter++;

		if(ir_counter >= 2000) {
			if(ir_level == 0 && pressed == 0) {
				// Button press
				current_message.command = COMMAND_STANDBY;
				handle_message(&current_message, 0);
				ir_counter = 0;
				pressed = 1;
			} else if(ir_level != 0 && state == STATE_SLEEPY) {
				state = STATE_SLEEPING;
			}
		}

		if(ir_bit_counter > 0 && ir_counter >= 127) {
			// Long pause - reset into non-listening mode
			ir_bit_counter = 0;
		}
	} else {
		pressed = 0;
		if(ir_counter > 3) {
			// Direction of transition indicates bit value
			// space to mark (ir_level=0) is a 1, mark to space is 0.
			current_message.data <<= 1;
			if(!ir_level)
				current_message.data |= 1;
			ir_bit_counter++;
			ir_counter = 0;
			
			if(ir_bit_counter == IR_MESSAGE_LENGTH) {
				uint8_t is_repeat = previous_message.data == current_message.data;
				// Only bother decoding the message if it's not a repeat
				// or the repeat countdown has expired.
				if((repeat_countdown == 0) || !is_repeat) {
					handle_message(&current_message, is_repeat);

					// Set a delay until next allowable repeat - longer for a first repeat
					repeat_countdown = is_repeat?1000:3000;
				}
				
				// Reset the bit clock and counter
				ir_bit_counter = 0;
				ir_counter = 127;
				
				// Store the previous toggle value
				previous_message.data = current_message.data;
			}
		} else {
			// This transition is between bits, ignore it
		}
		
		last_ir_level = ir_level;
	}
}

ISR(TIMER0_COMPA_vect) {
	static uint8_t row = 0;

	// Turn off the old row
	PORTD |= PORTD_ROWS;
	PORTA |= PORTA_ROWS;

	// Set the column data
	row = (row + 1) & 0x7;
	COLUMN_PORT = display[row];
	
	// Turn on the new row
	ENABLE_ROW(row);
}

static void ioinit(void) {
	// Enable timer 0: display refresh
	OCR0A = 250; // 8 megahertz / 64 / 125 = 500Hz
	TCCR0B = _BV(WGM01); // CTC(OCR0A), /64 prescaler
	
	OCR1A = 250; // 8 megahertz / 8 / 250 = 4KHz
	TCCR1B = _BV(WGM12) | _BV(CS11); // CTC(OCR1A), /8 prescaler

	TIMSK |= _BV(OCIE1A) | _BV(OCIE0A); // Interrupt on counter reset

	// PORTB is all output for columns
	COLUMN_DDR = 0xff;

	// Enable pullup on IR receiver pin
	PORTD |= _BV(IR_PIN);

	sei();
}

static uint8_t read_font_column(uint8_t character, uint8_t column) {
	return pgm_read_byte(font + (character * 5) + column);
}

static void shift_right(void) {
	for(int i = 7; i > 0; i--)
		display[i] = display[i - 1];
}

static void shift_left(void) {
	for(int i = 0; i < 7; i++)
		display[i] = display[i + 1];
}

static void draw_character(char ch) {
	for(int i = 0; i < 5; i++)
		display[i + 2] = read_font_column(ch, i);
}

void animate(void) {
	while(1) {
		uint8_t *dataptr = stored_config.data;
		for(int i = 0; i < config.mode.animate.framecount; i++) {
			if(state != STATE_NORMAL) return;
			for(int j = 0; j < 8; j++) {
				display[j] = eeprom_read_byte(dataptr++);
			}
			for(int j = 0; j < config.mode.animate.delay; j++)
				_delay_ms(10);
		}
	}
}

void marquee(void) {
	uint8_t *msgptr = stored_config.data;
	while(state == STATE_NORMAL) {
		uint8_t current = eeprom_read_byte(msgptr++);
		if(current == '\0') { 
			msgptr = stored_config.data;
			continue;
		}

		for(int j = 0; j < 5 + config.mode.marquee.spacing; j++) {
			shift_left();
			
			if(j >= 5) {
				// Empty columns at the end
				display[7] = 0;
			} else {
				// Display the next column of the current letter 
				display[7] = read_font_column(current, j);
			}

			for(int k = 0; k < config.mode.marquee.delay && !(keypresses & KEY_MENU); k++) {
				_delay_ms(10);
			}
		}
	}
}

void edit_marquee(void) {
	uint8_t idx = 0;
	char current;

	void write_dirty(void) {
		eeprom_update_byte((uint8_t*)&stored_config.data[idx], current);
	}
	
	void read_current(void) {
		current = eeprom_read_byte((uint8_t*)&stored_config.data[idx]);
	}
	
	display[0] = 0;
	display[1] = 0;
	display[7] = 0;
	
	// Load and show the first character
	read_current();
	draw_character(current);

	while(state == STATE_NORMAL) {
		if(keypresses & KEY_LEFT) {
			if(idx > 0) {
				write_dirty();
				idx--;
				read_current();
				for(int i = 0; i < 8; i++) {
					shift_right();
					if(i > 0 && i < 6) {
						display[0] = read_font_column(current, 5 - i);
					} else {
						display[0] = 0;
					}
					_delay_ms(50);
				}
			}
			keypresses &= ~KEY_LEFT;
		} else if(keypresses & KEY_RIGHT) {
			if(idx < MAX_DATA_LENGTH && current != '\0') {
				write_dirty();
				idx++;
				read_current();
				for(int i = 0; i < 8; i++) {
					shift_left();
					if(i > 1 && i < 7) {
						display[7] = read_font_column(current, i - 2);
					} else {
						display[7] = 0;
					}
					_delay_ms(50);
				}
			}
			keypresses &= ~KEY_RIGHT;
		} else if(keypresses & KEY_UP) {
			current++;
			draw_character(current);
			keypresses &= ~KEY_UP;
		} else if(keypresses & KEY_DOWN) {
			current--;
			draw_character(current);
			keypresses &= ~KEY_DOWN;
		} else {
			continue;
		}
	}
	write_dirty();
}

void edit(void) {
	if(config.flags & IS_ANIMATION) {
		state = STATE_MENU;
	} else {
		edit_marquee();
	}
}

void play(void) {
	if(config.flags & IS_ANIMATION) {
		animate();
	} else {
		marquee();
	}
}

mode_t modes[] = {
	{play, 0},
	{edit, 1},
	{NULL, 0},
	{}
};

static uint8_t read_glyph_column(uint8_t glyph_id, uint8_t column) {
	return pgm_read_byte(glyphs + glyph_id * 8 + column);
}

static void draw_glyph(uint8_t glyph_id) {
	// Show the current glyph
	for(uint8_t i = 0; i < 8; i++)
		display[i] = read_glyph_column(glyph_id, i);
}

void menu(void) {
	draw_glyph(modes[mode_id].glyph_id);
	while(state == STATE_MENU) {
		if(keypresses & KEY_LEFT) {
			keypresses &= ~KEY_LEFT;
			if(mode_id > 0) {
				mode_id--;
				
				uint8_t glyph_id = modes[mode_id].glyph_id;
				for(uint8_t i = 0; i < 8; i++) {
					shift_right();
					display[0] = read_glyph_column(glyph_id, 7 - i);
					_delay_ms(50);
				}
			}
		} else if(keypresses & KEY_RIGHT) {
			keypresses &= ~KEY_RIGHT;
			if(modes[mode_id + 1].run != NULL) {
				mode_id++;

				uint8_t glyph_id = modes[mode_id].glyph_id;
				for(uint8_t i = 0; i < 8; i++) {
					shift_left();
					display[7] = read_glyph_column(glyph_id, i);
					_delay_ms(50);
				}
			}
		} else if(keypresses & KEY_ENTER) {
			keypresses &= ~KEY_ENTER;
			state = STATE_NORMAL;
		}
	}
}

void main(void) __attribute__((noreturn));

void main(void) {
	eeprom_read_block(&config, &stored_config.config, sizeof(config_t));
	
	ioinit();

	PWM_ON();
	
	mode_id = 0;
	for(;;) {
		switch(state) {
		case STATE_MENU:
			// Show the menu
			menu();
			break;
		case STATE_NORMAL:
			// Run a standard function
			modes[mode_id].run();
			break;
		case STATE_SLEEPING:
			// Go to sleep
			enter_sleep();
			state = STATE_SLEEPY;
			break;
		case STATE_SLEEPY:
			// Wait to enter STATE_SLEEPING or STATE_NORMAL
			break;
		case STATE_SLAVE:
			// Just refresh the display
			break;
		}
	}
}
