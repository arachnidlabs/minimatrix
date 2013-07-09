#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif
#include <avr/pgmspace.h>
#include "SPI.h"

#ifndef _OPTILOADER_H
#define _OPTILOADER_H

#define FUSE_PROT 0			/* memory protection */
#define FUSE_LOW 1			/* Low fuse */
#define FUSE_HIGH 2			/* High fuse */
#define FUSE_EXT 3			/* Extended fuse */

// You may want to tweak these based on whether your chip is
// using an internal low-speed crystal
#define CLOCKSPEED_FUSES   SPI_CLOCK_DIV128 
#define CLOCKSPEED_FLASH   SPI_CLOCK_DIV8

#define LED_ERR 8
#define LED_PROGMODE A0

typedef struct image {
    char image_name[30];	       /* Ie "optiboot_diecimila.hex" */
    char image_chipname[12];	       /* ie "atmega168" */
    uint16_t image_chipsig;	       /* Low two bytes of signature */
    byte image_progfuses[5];	       /* fuses to set during programming */
    byte fusemask[4];
    uint16_t chipsize;
    byte image_pagesize;	       /* page size for flash programming */
    const unsigned char *image_data;	               /* binary image data */
} image_t;

typedef struct alias {
  char image_chipname[12];
  uint16_t image_chipsig;
  image_t * alias_image;
} alias_t;

// Useful message printing definitions

#define debug(string) //flashprint(PSTR(string));


void pulse (int pin1, int pin2, int pinoff, int times);
void flashprint (const char p[]);
void led_on(int pin1, int pin2, int pinoff);
void leds_off(int pin1, int pin2, int pin3);

uint16_t spi_transaction (uint8_t a, uint8_t b, uint8_t c, uint8_t d);
image_t *findImage (char *name);


uint16_t readSignature (void);
boolean programFuses (const byte *fuses);
void eraseChip(void);
boolean verifyImage (const unsigned char *hextext, int imagesize);
void busyWait(void);
boolean flashPage (byte *pagebuff, uint16_t pageaddr, uint8_t pagesize);
boolean programImage(const unsigned char*, int, int);
byte hexton (byte h);
byte *readImagePage (byte *hextext, uint16_t pageaddr, uint8_t pagesize, byte *page);
boolean verifyFuses (const byte *fuses, const byte *fusemask);
void error(char *string);

#endif
