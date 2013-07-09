#include <avr/pgmspace.h>
#include "optiLoader.h"
#include "main_data.h"
#include "test_data.h"

image_t PROGMEM image_main_bin = {
    {"minimatrix.hex"},
    {"attiny4313"},
    0x920d,				/* Signature bytes for 328P */
    {0x3F, 0xE4, 0xDF, 0xFF},            // pre program fuses (prot/lock, low, high, ext)
    {0x3F, 0xFF, 0xFF, 0xFF},           // fuse mask
    sizeof(main_bin),     // size of image in bytes
    64,   // size in bytes of flash page
    main_bin,
};

image_t PROGMEM image_main_eebin = {
    {"minimatrix.eep"},
    {"attiny4313"},
    0x920d,				/* Signature bytes for 328P */
    {0x3F, 0xE4, 0xDF, 0xFF},            // pre program fuses (prot/lock, low, high, ext)
    {0x3F, 0xFF, 0xFF, 0xFF},           // fuse mask
    sizeof(main_eebin),     // size of image in bytes
    64,   // size in bytes of flash page
    main_eebin,
};

image_t PROGMEM image_test_bin = {
    {"test.hex"},
    {"attiny4313"},
    0x920d,				/* Signature bytes for 328P */
    {0x3F, 0xE4, 0xDF, 0xFF},            // pre program fuses (prot/lock, low, high, ext)
    {0x3F, 0xFF, 0xFF, 0xFF},           // fuse mask
    sizeof(test_bin),     // size of image in bytes
    64,   // size in bytes of flash page
    test_bin,
};

/*
 * Table of defined images
 */
image_t *images[] = {
  &image_main_bin,
  &image_main_eebin,
  &image_test_bin,
};

uint8_t NUMIMAGES = sizeof(images)/sizeof(images[0]);
