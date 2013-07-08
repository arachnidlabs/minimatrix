// Standalone AVR ISP programmer
// Modified July 2013 by Nick Johnson <nick@arachnidlabs.com>
// August 2011 by Limor Fried / Ladyada / Adafruit
// Jan 2011 by Bill Westfield ("WestfW")
//
// this sketch allows an Arduino to program a flash program
// into any AVR if you can fit the HEX file into program memory
// No computer is necessary. Two LEDs for status notification
// Press button to program a new chip. Piezo beeper for error/success 
// This is ideal for very fast mass-programming of chips!
//
// It is based on AVRISP
//
// using the following pins:
// 10: slave reset
// 11: MOSI
// 12: MISO
// 13: SCK
// ----------------------------------------------------------------------


#include "optiLoader.h"
#include "SPI.h"

// Global Variables
int pmode=0;
byte pageBuffer[64];		       /* One page of flash */


/*
 * Pins to target
 */
#define SCK 13
#define MISO 12
#define MOSI 11
#define RESET 10
#define TARGET_POWER 9

#define PIN_BATT_NEG 2
#define PIN_BATT_POS 4
#define PIN_IRVCC 3
#define PIN_IROUT 5
#define PIN_SRLATCH 6
#define PIN_SRCLK 7
#define PIN_SRDATA 8

#define ISP_A A2, A1, A3
#define ISP_B A1, A2, A3
#define TEST_A A1, A0, A2
#define TEST_B A0, A1, A2
#define PROG_A A2, A0, A1
#define PROG_B A0, A2, A1

void set_prescaler(int prescaler) {
  cli();
  CLKPR = _BV(CLKPCE);
  CLKPR = prescaler;
  sei();
}

void setup () {
  Serial.begin(57600);			/* Initialize serial for status msgs */
  Serial.println("\nMatrixjig Bootstrap programmer (originally OptiLoader Bill Westfield (WestfW))");

  pinMode(TARGET_POWER, OUTPUT);

  pulse(ISP_A, 2);
  pulse(TEST_A, 2);
  pulse(PROG_A, 2);
  
}

void loop (void) {
  digitalWrite(TARGET_POWER, HIGH);			/* Turn on target power */

  Serial.println("\nType 'G' or hit BUTTON for next chip");
  while (1) {
    if  (Serial.read() == 'G')
      break;  
    if(digitalRead(PIN_BATT_POS))
      break;
  }
  delay(1000);
  
  led_on(ISP_A);
  set_prescaler(0x1);
  target_poweron();

  uint16_t signature;
  image_t *targetimage;
        
  if (! (signature = readSignature()))	// Figure out what kind of CPU
    error("Signature fail", ISP_B);
  
  if (! (targetimage = findImage(signature)))	// look for an image
    error("Image fail", ISP_B);
  
  eraseChip();

  if (! programFuses(targetimage->image_progfuses)) 	// get fuses ready to program
    error("Programming Fuses fail", ISP_B);
    
  
  if (! verifyFuses(targetimage->image_progfuses, targetimage->fusemask) )
    error("Failed to verify fuses", ISP_B);

  set_prescaler(0x0);

  end_pmode();
  led_on(TEST_A);
  start_pmode();

  byte *hextext = targetimage->image_hexcode;  
  uint16_t pageaddr = 0;
  uint8_t pagesize = pgm_read_byte(&targetimage->image_pagesize);
  uint16_t chipsize = pgm_read_word(&targetimage->chipsize);
        
  //Serial.println(chipsize, DEC);
  while (hextext != NULL) {
     byte *hextextpos = readImagePage (hextext, pageaddr, pagesize, pageBuffer, TEST_B);
          
     boolean blankpage = true;
     for (uint8_t i=0; i<pagesize; i++) {
       if (pageBuffer[i] != 0xFF) blankpage = false;
     }          
     if (! blankpage) {
       if (! flashPage(pageBuffer, pageaddr, pagesize))	
	 error("Flash programming failed", TEST_B);
     }
     hextext = hextextpos;
     pageaddr += pagesize;
  }
  
  // Set fuses to 'final' state
  //if (! programFuses(targetimage->image_normfuses))
  //  error("Programming Fuses fail", TEST_B);
    
  end_pmode();
  led_on(PROG_A);
  start_pmode();
  
  Serial.println("\nVerifing flash...");
  if (! verifyImage(targetimage->image_hexcode, PROG_B) ) {
    error("Failed to verify chip", PROG_B);
  } else {
    Serial.println("\tFlash verified correctly!");
  }

  if (! verifyFuses(targetimage->image_normfuses, targetimage->fusemask) ) {
    error("Failed to verify fuses", PROG_B);
  } else {
    Serial.println("Fuses verified correctly!");
  }
  
  while(digitalRead(PIN_BATT_POS));
 
  target_poweroff();			/* turn power off */
  leds_off(ISP_A);
  delay(1000);
}

void program_chip(image_t *targetimage) {
  uint16_t signature;
  image_t *targetimage;

  set_prescaler(0x1);
        
  if (! (signature = readSignature()))	// Figure out what kind of CPU
    error("Signature fail", ISP_B);

  if(signature != targetimage->image_chipsig)
    error("Signature does not match");

  eraseChip();

  if (! programFuses(targetimage->image_progfuses)) 	// get fuses ready to program
    error("Programming Fuses fail", ISP_B);
    
  
  if (! verifyFuses(targetimage->image_progfuses, targetimage->fusemask) )
    error("Failed to verify fuses", ISP_B);

  set_prescaler(0x0);

  end_pmode();
  start_pmode();

  byte *hextext = targetimage->image_hexcode;  
  uint16_t pageaddr = 0;
  uint8_t pagesize = pgm_read_byte(&targetimage->image_pagesize);
  uint16_t chipsize = pgm_read_word(&targetimage->chipsize);
        
  while (hextext != NULL) {
     byte *hextextpos = readImagePage (hextext, pageaddr, pagesize, pageBuffer, TEST_B);
          
     boolean blankpage = true;
     for (uint8_t i=0; i<pagesize; i++) {
       if (pageBuffer[i] != 0xFF) blankpage = false;
     }          
     if (! blankpage) {
       if (! flashPage(pageBuffer, pageaddr, pagesize))	
	 error("Flash programming failed", TEST_B);
     }
     hextext = hextextpos;
     pageaddr += pagesize;
  }
  
  end_pmode();
  start_pmode();
  
  Serial.println("\nVerifing flash...");
  if (! verifyImage(targetimage->image_hexcode, PROG_B) ) {
    error("Failed to verify chip", PROG_B);
  } else {
    Serial.println("\tFlash verified correctly!");
  }

  if (! verifyFuses(targetimage->image_normfuses, targetimage->fusemask) ) {
    error("Failed to verify fuses", PROG_B);
  } else {
    Serial.println("Fuses verified correctly!");
  }
}


void error(char *string, int pin1, int pin2, int pinoff) { 
  Serial.println(string); 
  led_on(pin1, pin2, pinoff);
  while(1);
}

void start_pmode () {
  pinMode(13, INPUT); // restore to default

  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV128); 
  
  debug("...spi_init done");
  // following delays may not work on all targets...
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, HIGH);
  pinMode(SCK, OUTPUT);
  digitalWrite(SCK, LOW);
  delay(50);
  digitalWrite(RESET, LOW);
  delay(50);
  pinMode(MISO, INPUT);
  pinMode(MOSI, OUTPUT);
  debug("...spi_transaction");
  spi_transaction(0xAC, 0x53, 0x00, 0x00);
  debug("...Done");
  pmode = 1;
}

void end_pmode () {
  SPCR = 0;				/* reset SPI */
  digitalWrite(MISO, 0);		/* Make sure pullups are off too */
  pinMode(MISO, INPUT);
  digitalWrite(MOSI, 0);
  pinMode(MOSI, INPUT);
  digitalWrite(SCK, 0);
  pinMode(SCK, INPUT);
  digitalWrite(RESET, 0);
  pinMode(RESET, INPUT);
  pmode = 0;
}


/*
 * target_poweron
 * begin programming
 */
boolean target_poweron ()
{
  digitalWrite(RESET, LOW);  // reset it right away.
  pinMode(RESET, OUTPUT);
//  digitalWrite(TARGET_POWER, HIGH);
//  delay(100);
  Serial.print("Starting Program Mode");
  start_pmode();
  Serial.println(" [OK]");
  return true;
}

boolean target_poweroff ()
{
  end_pmode();
  digitalWrite(TARGET_POWER, LOW);
  return true;
}

