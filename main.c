
#include <stdlib.h>
#include <string.h>

#include <stdlib.h>
#include <string.h>

// include NESLIB header
#include "neslib.h"

// include CC65 NES Header (PPU)
#include <nes.h>

// link the pattern table into CHR ROM
//#link "chr_generic.s"

// BCD arithmetic support
#include "bcd.h"
//#link "bcd.c"

// VRAM update buffer
#include "vrambuf.h"
//#link "vrambuf.c"

/*{pal:"nes",layout:"nes"}*/
const char PALETTE[32] = { 
  0x01,			// screen color

  0x11,0x30,0x27,0x00,	// background palette 0
  0x1C,0x20,0x2C,0x00,	// background palette 1
  0x00,0x10,0x20,0x00,	// background palette 2
  0x06,0x16,0x26,0x00,   // background palette 3

  0x16,0x35,0x24,0x00,	// sprite palette 0
  0x00,0x37,0x25,0x00,	// sprite palette 1
  0x0D,0x2D,0x3A,0x00,	// sprite palette 2
  0x0D,0x27,0x2A	// sprite palette 3
};

#define BYTES_PER_INSTRUCTION	2
#define INSTRUCTIONS_PER_BLOCK  16
#define NUMBER_OF_BLOCKS        16

#define MEM_BYTES (BYTES_PER_INSTRUCTION * INSTRUCTIONS_PER_BLOCK * NUMBER_OF_BLOCKS)

static unsigned char program_memory[MEM_BYTES];
static unsigned char program_block_flags[NUMBER_OF_BLOCKS];




// setup PPU and tables
void setup_graphics() {
  // clear sprites
  oam_clear();
  // set palette colors
  pal_all(PALETTE);
}

void reset_memory()
{
  short i;
  for (i=0;i<MEM_BYTES;i++) {
    program_memory[i] = 0;
  }
  
  for (i=0;i<NUMBER_OF_BLOCKS;i++) {
  	program_block_flags[i]=0;
  }
}

void main(void)
{  
  setup_graphics();
  // enable rendering
  ppu_on_all();
  // infinite loop
  reset_memory();
  while(1) {
  }
}
