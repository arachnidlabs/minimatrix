#include <avr/pgmspace.h>
#include "optiLoader.h"

/*
 * Bootload images.
 * These are the intel Hex files produced by the optiboot makefile,
 * with a small amount of automatic editing to turn them into C strings,
 * and a header attched to identify them
 */

extern image_t *images[];
extern uint8_t NUMIMAGES;

/*
 * readSignature
 * read the bottom two signature bytes (if possible) and return them
 * Note that the highest signature byte is the same over all AVRs so we skip it
 */

uint16_t readSignature (void)
{
  SPI.setClockDivider(CLOCKSPEED_FUSES); 
    
  uint16_t target_type = 0;
  Serial.print("\nReading signature:");
  
  target_type = spi_transaction(0x30, 0x00, 0x01, 0x00);
  target_type <<= 8;
  target_type |= spi_transaction(0x30, 0x00, 0x02, 0x00);
  
  Serial.println(target_type, HEX);
  if (target_type == 0 || target_type == 0xFFFF) {
    if (target_type == 0) {
      Serial.println("  (no target attached?)");
    }
  }
  return target_type;
}

/*
 * findImage
 *
 * given 'name' containing the image name,
 * search the hex images that we have programmed in flash, looking for one
 * that matches.
 */
image_t *findImage (char *name)
{
  image_t *ip;
  Serial.println("Searching for image...");

  for (byte i=0; i < NUMIMAGES; i++) {
    ip = images[i];

    if(ip && strcmp_P(name, (const prog_char*)&ip->image_name) == 0) {
	Serial.print("  Found \"");
	flashprint(&ip->image_name[0]);
	Serial.print("\" for ");
	flashprint(&ip->image_chipname[0]);
	Serial.println();

	return ip;
    }
  }
  Serial.println(" Not Found");
  return 0;
}

/*
 * programmingFuses
 * Program the fuse/lock bits
 */
boolean programFuses (const byte *fuses)
{
  SPI.setClockDivider(CLOCKSPEED_FUSES); 
    
  byte f;
  Serial.print("\nSetting fuses");

  f = pgm_read_byte(&fuses[FUSE_PROT]);
  if (f) {
    Serial.print("\n  Set Lock Fuse to: ");
    Serial.print(f, HEX);
    Serial.print(" -> ");
    Serial.print(spi_transaction(0xAC, 0xE0, 0x00, f), HEX);
  }
  delay(10);
  f = pgm_read_byte(&fuses[FUSE_LOW]);
  if (f) {
    Serial.print("  Set Low Fuse to: ");
    Serial.print(f, HEX);
    Serial.print(" -> ");
    Serial.print(spi_transaction(0xAC, 0xA0, 0x00, f), HEX);
  }
  delay(10);
  f = pgm_read_byte(&fuses[FUSE_HIGH]);
  if (f) {
    Serial.print("  Set High Fuse to: ");
    Serial.print(f, HEX);
    Serial.print(" -> ");
    Serial.print(spi_transaction(0xAC, 0xA8, 0x00, f), HEX);
  }
  delay(10);
  f = pgm_read_byte(&fuses[FUSE_EXT]);
  if (f) {
    Serial.print("  Set Ext Fuse to: ");
    Serial.print(f, HEX);
    Serial.print(" -> ");
    Serial.print(spi_transaction(0xAC, 0xA4, 0x00, f), HEX);
  }
  Serial.println();
  return true;			/* */
}

/*
 * verifyFuses
 * Verifies a fuse set
 */
boolean verifyFuses (const byte *fuses, const byte *fusemask)
{
  SPI.setClockDivider(CLOCKSPEED_FUSES); 
  byte f;
  Serial.println("Verifying fuses...");
  f = pgm_read_byte(&fuses[FUSE_PROT]);
  if (f) {
    uint8_t readfuse = spi_transaction(0x58, 0x00, 0x00, 0x00);  // lock fuse
    readfuse &= pgm_read_byte(&fusemask[FUSE_PROT]);
    Serial.print("\tLock Fuse: "); Serial.print(f, HEX);  Serial.print(" is "); Serial.print(readfuse, HEX);
    if (readfuse != f) 
      return false;
  }
  f = pgm_read_byte(&fuses[FUSE_LOW]);
  if (f) {
    uint8_t readfuse = spi_transaction(0x50, 0x00, 0x00, 0x00);  // low fuse
    Serial.print("\tLow Fuse: 0x");  Serial.print(f, HEX); Serial.print(" is 0x"); Serial.print(readfuse, HEX);
    readfuse &= pgm_read_byte(&fusemask[FUSE_LOW]);
    if (readfuse != f) 
      return false;
  }
  f = pgm_read_byte(&fuses[FUSE_HIGH]);
  if (f) {
    uint8_t readfuse = spi_transaction(0x58, 0x08, 0x00, 0x00);  // high fuse
    readfuse &= pgm_read_byte(&fusemask[FUSE_HIGH]);
    Serial.print("\tHigh Fuse: 0x");  Serial.print(f, HEX); Serial.print(" is 0x");  Serial.print(readfuse, HEX);
    if (readfuse != f) 
      return false;
  }
  f = pgm_read_byte(&fuses[FUSE_EXT]);
  if (f) {
    uint8_t readfuse = spi_transaction(0x50, 0x08, 0x00, 0x00);  // ext fuse
    readfuse &= pgm_read_byte(&fusemask[FUSE_EXT]);
    Serial.print("\tExt Fuse: 0x"); Serial.print(f, HEX); Serial.print(" is 0x"); Serial.print(readfuse, HEX);
    if (readfuse != f) 
      return false;
  }
  Serial.println();
  return true;			/* */
}

// Send one byte to the page buffer on the chip
void flashWord (uint8_t hilo, uint16_t addr, uint8_t data) {
#if VERBOSE
  Serial.print(data, HEX);  Serial.print(':');
  Serial.print(spi_transaction(0x40+8*hilo,  addr>>8 & 0xFF, addr & 0xFF, data), HEX);
  Serial.print(" ");
#else
  spi_transaction(0x40+8*hilo, addr>>8 & 0xFF, addr & 0xFF, data);
#endif
}

// Basically, write the pagebuff (with pagesize bytes in it) into page $pageaddr
boolean flashPage (byte *pagebuff, uint16_t pageaddr, uint8_t pagesize) {  
  SPI.setClockDivider(CLOCKSPEED_FLASH); 


  Serial.print("Flashing page "); Serial.println(pageaddr, HEX);
  for (uint16_t i=0; i < pagesize/2; i++) {
    
#if VERBOSE
    Serial.print(pagebuff[2*i], HEX); Serial.print(' ');
    Serial.print(pagebuff[2*i+1], HEX); Serial.print(' ');
    if ( i % 16 == 15) Serial.println();
#endif

    flashWord(LOW, i, pagebuff[2*i]);
    flashWord(HIGH, i, pagebuff[2*i+1]);
  }

  // page addr is in bytes, byt we need to convert to words (/2)
  pageaddr = (pageaddr/2);

  uint16_t commitreply = spi_transaction(0x4C, (pageaddr >> 8) & 0xFF, pageaddr & 0xFF, 0);

  Serial.print("  Commit Page: 0x");  Serial.print(pageaddr, HEX);
  Serial.print(" -> 0x"); Serial.println(commitreply, HEX);
  if (commitreply != pageaddr) 
    return false;

  busyWait();
  
  return true;
}

boolean programImage(const unsigned char *imagedata, int pagesize, int chipsize) {
  unsigned char pageBuffer[pagesize];
  
  for(int pos = 0; pos < chipsize; pos += pagesize) {
     Serial.print("Flashing starting at ");
     Serial.println(pos, HEX);
     memcpy_P(pageBuffer, &imagedata[pos], pagesize);
          
     boolean blankpage = true;
     for (uint8_t i=0; i<pagesize; i++) {
       if (pageBuffer[i] != 0xFF) blankpage = false;
     }          
     if (! blankpage) {
       if (! flashPage(pageBuffer, pos, pagesize)) {
	 error("Flash programming failed");
         return false;
       }
     }
  }
  return true;
}

// verifyImage does a byte-by-byte verify of the flash hex against the chip
// Thankfully this does not have to be done by pages!
// returns true if the image is the same as the hextext, returns false on any error
boolean verifyImage (const unsigned char *image, int imagesize)  {
  uint16_t address = 0;
  
  SPI.setClockDivider(CLOCKSPEED_FLASH); 

  uint16_t len;
  byte b;

  for(int i = 0; i < imagesize; i++) {
    b = pgm_read_byte(&image[i]);

    // verify this byte!
    if (i % 2) {
      // for 'high' bytes:
      if (b != (spi_transaction(0x28, i >> 9, i / 2, 0) & 0xFF)) {
        Serial.print("verification error at address 0x"); Serial.print(i, HEX);
        Serial.print(" Should be 0x"); Serial.print(b, HEX); Serial.print(" not 0x");
        Serial.println((spi_transaction(0x28, i >> 9, i / 2, 0) & 0xFF), HEX);
        return false;
      }
    } else {
      // for 'low bytes'
      if (b != (spi_transaction(0x20, i >> 9, i / 2, 0) & 0xFF)) {
        Serial.print("verification error at address 0x"); Serial.print(i, HEX);
        Serial.print(" Should be 0x"); Serial.print(b, HEX); Serial.print(" not 0x");
        Serial.println((spi_transaction(0x20, i >> 9, i / 2, 0) & 0xFF), HEX);
        return false;
      }
    } 
  }
  return true;
}

boolean programEEPROM(const unsigned char *imagedata, int pagesize, int imagesize) {
  for(int idx = 0; idx < imagesize; idx += pagesize) {
    for(int i = 0; (i < pagesize) && (idx + i < imagesize); i++) {
      spi_transaction(0xC1, 0x00, i, pgm_read_byte(&imagedata[idx + i]));
    }
    spi_transaction(0xC2, idx >> 8, idx & 0xFC, 0x00);
    busyWait();
  }
  return true;
}

boolean verifyEEPROM(const unsigned char *imagedata, int imagesize) {
  for(int idx = 0; idx < imagesize; idx++) {
    byte b = pgm_read_byte(&imagedata[idx]);
    if(b != spi_transaction(0xA0, idx >> 8, idx & 0xFF, 0x00) & 0xFF) {
        Serial.print("EEPROM verification error at address 0x"); Serial.print(idx, HEX);
        Serial.print(" Should be 0x"); Serial.print(b, HEX); Serial.print(" not 0x");
        Serial.println((spi_transaction(0xA0, idx >> 8, idx & 0xFF, 0x00) & 0xFF), HEX);
        return false;      
    }
  }
  return true;
}

// Send the erase command, then busy wait until the chip is erased

void eraseChip(void) {
  SPI.setClockDivider(CLOCKSPEED_FUSES); 
    
  spi_transaction(0xAC, 0x80, 0, 0);	// chip erase    
  busyWait();
}

// Simply polls the chip until it is not busy any more - for erasing and programming
void busyWait(void)  {
  byte busybit;
  do {
    busybit = spi_transaction(0xF0, 0x0, 0x0, 0x0);
    //Serial.print(busybit, HEX);
  } while (busybit & 0x01);
}


/*
 * Functions specific to ISP programming of an AVR
 */
uint16_t spi_transaction (uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  uint8_t n, m;
  SPI.transfer(a); 
  n = SPI.transfer(b);
  //if (n != a) error = -1;
  m = SPI.transfer(c);
  return 0xFFFFFF & ((n<<16)+(m<<8) + SPI.transfer(d));
}

