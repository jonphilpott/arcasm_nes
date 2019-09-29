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
  0x02,			// screen color

  0x2A,0x16,0x30,0x00,	// background palette 0
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

static byte program_memory_updated = 0;

struct player_state {
#define PLAYER_STATE_PICK_BLOCK 1
#define PLAYER_STATE_PICK_BYTE  2
#define PLAYER_STATE_ALTER_BYTE 3
	unsigned char state;
  	unsigned char current_block;
  	unsigned int  score;
  
  	byte x_pos, y_pos;
};

static struct player_state players[2];

#define GAME_STATE_INTRO    0
#define GAME_STATE_GAME     1
#define GAME_STATE_GAMEOVER 2

static unsigned char game_state = 1;

struct cpu_regs {
  unsigned char a, b, x, y, pc;
};

static struct cpu_regs cpu_threads[2];

static unsigned char lfsr = 0x55;

unsigned char get_random_byte(unsigned char rounds)
{ 
  unsigned char out;
  while (rounds--) {
  	lfsr = (lfsr >> 1) | 
    		((((lfsr >> 7) ^ (lfsr >> 5) ^ (lfsr >> 4) ^ (lfsr >> 3)) & 1) << 7);
    	out = (out << 1) | (lfsr & 1);
  }
  
  return out;
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
  players[1].current_block = 4;
  players[0].score = players[1].score = 0;
  
  memfill(&cpu_threads[0], 0, sizeof(struct cpu_regs));
  memfill(&cpu_threads[1], 0, sizeof(struct cpu_regs));
}

void cpu_mem_write(unsigned char own, unsigned char addr, unsigned char val)
{
  if (own != 3) 
  	players[own].score += PLAYER_SCORE_MEM_WRITE;
  
  if ((addr & 1) == 0) {
    val = (own << 6) | (val & 0x7F);
  }
  
  program_memory[addr] = val;
  
  program_memory_updated = 1;
}

// use top two bits to track ownership of writen code.
// has downsides.. but these could be taken advantage of.
// .. and why not
void cpu_tick(char thread)
{
  struct cpu_regs *t = &cpu_threads[thread];
  // shave off 1 bit, PC accesses should always be even (nice try)
  unsigned char pc     = t->pc &  0xFE;
  unsigned char opcode = program_memory[pc];
  unsigned char arg    = program_memory[pc + 1];
  unsigned char owner  = opcode >> 6;

  unsigned char pc_mod = 0;
  
  switch (opcode) {
  #define OPCODE_NOP 0
    case OPCODE_NOP:
      	break;
  #define OPCODE_LDA 1
    case OPCODE_LDA:
    	t->a = arg;
      	break;
  #define OPCODE_STA 2
    case OPCODE_STA:
      	cpu_mem_write(owner, arg, t->a);
    	break;
  #define OPCODE_HOP 3
    case OPCODE_HOP:
      	pc_mod = 1;
      	t->pc += (signed char) arg;
      	break;
  #define OPCODE_JMP 4
    case OPCODE_JMP:
      	t->pc = arg;
      	pc_mod = 1;
      	break;
  #define OPCODE_ZHOP 5
    case OPCODE_ZHOP:
        if (t->a == 0) {
          pc_mod = 1;
          t->pc += 4;
        }
    break;
  #define OPCODE_WLD 6
    case OPCODE_WLD:
    	t->x = program_memory[t->a];
        t->y = program_memory[t->a + 1];
        break;
  #define OPCODE_WCP 7
    case OPCODE_WCP:
      	program_memory[t->a] = t->x;
      	program_memory[t->a + 1] = t->y;
        break;
  #define OPCODE_MEMW 8
    case OPCODE_MEMW:
      	pc_mod = 1;
      	if (program_memory[arg] == t->a) {
        	t->pc += 2;
        }
        break;
    default:
        t->pc = 0;
  }
  
  if (pc_mod == 0) {
  	t->pc += 2;
  }
}



void clrscr()
{
  ppu_off();
  vram_adr(0x2000);
  vram_fill(0, 32*28);
  vram_adr(0x0);
  ppu_on_all();
}

void title_screen(void)
{
  unsigned char by1 = 0;
  unsigned char by2 = 0;
  clrscr();
  
  vram_adr(NTADR_A(9,12));
  vram_write("ARCASM", 6);
  vram_adr(NTADR_A(9, 14));
  vram_write("PRESS START", 12);

  vram_adr(NTADR_A(10, 10));
  
  while (1) {
    	by1 = get_random_byte(8);
   	by2 = pad_trigger(0) | pad_trigger(1);
    	if (by2 & PAD_START) break;
  }
}

void gameover_screen(void)
{
  clrscr();

  ppu_wait_frame();
  vram_adr(NTADR_A(9,12));
  vram_write("GAME OVER", 9);
  vram_adr(NTADR_A(9, 14));
  vram_write("PRESS START", 12);
  
  while (1) {
    	if (pad_trigger(0) & PAD_START) break;
  }
}



void draw_mem(byte sx, byte sy, struct player_state *p)
{
  byte i = 0;
  byte addr = 0;
  byte opcode = 0;
  byte arg = 0;
  byte owner = 0;
  static unsigned char line[8];
    
  for (i = 0; i < 16 ; i++) 
  {
    addr =   (p->current_block * 32) + (i * 2);
    opcode = program_memory[addr];
    arg    = program_memory[(addr + 1) & 0xFF];
    owner  = opcode >> 6;
    
    line[0] = 1 + (addr >> 4);
    line[1] = 1 + (addr & 0xF);
    line[2] = 0x11 + owner;
    line[3] = 1 + (opcode >> 4);
    line[4] = 1 + (opcode & 0xF);
    line[5] = 0x3A;
    line[6] = 1 + (arg >> 4);
    line[7] = 1 + (arg & 0xF); 
  
    vrambuf_put(NTADR_A(sx, sy+i), line, 8);
    
    if (i == 8) {
    	ppu_wait_nmi();
      	vrambuf_clear();
    }
  }
  
  ppu_wait_nmi();
  vrambuf_clear();
}

//void draw_blocks();
//void draw_player_sprite(struct player_state *p);
//void handle_player_input(struct player_state *p);
//void handle_enemies();
//void maybe_cpu_tick();


void game_loop(void) 
{
  clrscr();
  
  reset_memory();
  
  // clear vram buffer
  vrambuf_clear();
  
  // set NMI handler
  set_vram_update(updbuf);
    
  draw_mem(1,  9, &players[0]);
  draw_mem(23, 9, &players[1]);

  while (1) 
  {
    	if (program_memory_updated) {
           draw_mem(1,  9, &players[0]);
           draw_mem(23, 9, &players[1]);
           program_memory_updated = 0;
        }
    
        if (pad_trigger(0) & PAD_START) {
            cpu_mem_write(3, 0, program_memory[0] + 1);
        }
  }
}

void main(void)
{  
  unsigned char foo = 0;

  setup_graphics();

  ppu_on_all(); 
  
  // main loop
  while(1) {
    foo = get_random_byte(2);
    switch (game_state) 
    {
      case GAME_STATE_INTRO:
        title_screen();
        game_state = GAME_STATE_GAME;
        break;
      case GAME_STATE_GAME:
        game_loop();
        break;
      case GAME_STATE_GAMEOVER:
        gameover_screen();
        game_state = GAME_STATE_INTRO;
        break;
    }
  }
}
