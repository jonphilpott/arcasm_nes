
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

struct player_state {
#define PLAYER_STATE_PICK_BLOCK 1
#define PLAYER_STATE_PICK_BYTE  2
#define PLAYER_STATE_ALTER_BYTE 3
	unsigned char state;
  	unsigned char current_block;
  	unsigned char current_byte;
  	unsigned int  score;
};

static struct player_state player1, player2;

#define GAME_STATE_INTRO    0
#define GAME_STATE_GAME     1
#define GAME_STATE_GAMEOVER 2

static unsigned char game_state = 0;

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
  
  player1.state = player2.state = PLAYER_STATE_PICK_BLOCK;
  player1.current_block = 0;
  player2.current_block = (NUMBER_OF_BLOCKS) >> 1;
  player1.score = player2.score = 0;
  player1.current_byte = player2.current_byte = 0;
}

void main(void)
{  
  setup_graphics();
  // enable rendering
  ppu_on_all();
  
  reset_memory();
  
  // main loop
  while(1) {
    switch (game_state) 
    {
      case GAME_STATE_INTRO:
        break;
      case GAME_STATE_GAME:
        break;
      case GAME_STATE_GAMEOVER:
        break;
    }
  }
}
