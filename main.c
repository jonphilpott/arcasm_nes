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

#define PLAYER_SCORE_MEM_WRITE (128)

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

static struct player_state players[2];

#define GAME_STATE_INTRO    0
#define GAME_STATE_GAME     1
#define GAME_STATE_GAMEOVER 2

static unsigned char game_state = 0;

struct cpu_regs {
  unsigned char a, b, x, y, pc;
};

static struct cpu_regs cpu_threads[2];

static char lfsr = 0x55;

unsigned char get_random_byte(unsigned char rounds)
{  
  while (rounds--) {
  	lfsr = (lfsr >> 1) | 
    		((((lfsr >> 7) ^ (lfsr >> 5) ^ (lfsr >> 4) ^ (lfsr >> 3)) & 1) << 7);
  }
  
  return (unsigned char) lfsr;
}


// setup PPU and tables
void setup_graphics() {
  // clear sprites
  oam_clear();
  // set palette colors
  pal_all(PALETTE);
}

void reset_memory()
{
  memfill(program_memory, 0, MEM_BYTES);
  memfill(program_block_flags, 0, NUMBER_OF_BLOCKS);
  
  players[0].state = players[1].state = PLAYER_STATE_PICK_BLOCK;
  players[0].current_block = 0;
  players[1].current_block = (NUMBER_OF_BLOCKS) >> 1;
  players[0].score = players[1].score = 0;
  players[0].current_byte = players[1].current_byte = 0;
  
  memfill(&cpu_threads[0], 0, sizeof(struct cpu_regs));
  memfill(&cpu_threads[1], 0, sizeof(struct cpu_regs));
}

void cpu_mem_write(unsigned char own, unsigned char addr, unsigned char val)
{
  if (own != 3) 
  	players[own].score += PLAYER_SCORE_MEM_WRITE;
  program_memory[addr] = val;
}

// use top two bits to track ownership of writen code.
// has downsides.. but these could be taken advantage of.
// .. and why not
void cpu_tick(char thread)
{
  struct cpu_regs *t = &cpu_threads[thread];
  // only pull even PCs and shave off last two bits
  unsigned char p      = t->pc &  0x3E;
  unsigned char owner  = t->pc >> 6;
  unsigned char opcode = program_memory[p];
  unsigned char arg    = program_memory[p + 1];
  unsigned char pc_mod = 0;
  

  
  switch (opcode) {
  #define OPCODE_NOP 00
    case OPCODE_NOP:
      	break;
  #define OPCODE_LDA 01
    case OPCODE_LDA:
    	t->a = arg;
      	break;
  #define OPCODE_STA 02
    case OPCODE_STA:
      	cpu_mem_write(owner, arg, t->a);
    	break;
  #define OPCODE_HOP 03
    case OPCODE_HOP:
      	pc_mod = 1;
      	t->pc += (signed char) arg;
      	break;
  #define OPCODE_JMP 04
    case OPCODE_JMP:
      	t->pc = arg;
      	pc_mod = 1;
      	break;
    default:
        t->pc = 0;
  }
  
  if (pc_mod == 0) {
  	t->pc += 2;
  }
}

void main(void)
{  
  unsigned char foo = 0;

  setup_graphics();
  // enable rendering
  ppu_on_all();
  
  reset_memory();
  
  // main loop
  while(1) {
    foo = get_random_byte(2);
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
