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

#include "apu.h"
//#link "apu.c"


/*{pal:"nes",layout:"nes"}*/
const char PALETTE[32] =
  { 
   0x12,			// screen color
   
   0x2A,0x00,0x30,0x00,	// background palette 0
   0x1C,0x20,0x2C,0x00,	// background palette 1
   0x00,0x10,0x20,0x00,	// background palette 2
   0x06,0x16,0x26,0x00,   // background palette 3

   0x16,0x35,0x24,0x00,	// sprite palette 0
   0x00,0x37,0x25,0x00,	// sprite palette 1
   0x0D,0x2D,0x3A,0x00,	// sprite palette 2
   0x0D,0x27,0x2A	// sprite palette 3
  };

#define PLAYER_SCORE_CPU_TAKEOVER (128)
#define PLAYER_SCORE_MEM_WRITE (128)
#define PLAYER_SCORE_CURSOR_DESTROY (64)
#define PLAYER_SCORE_CURSOR_MEMEDIT (8)

#define BYTES_PER_INSTRUCTION	2
#define INSTRUCTIONS_PER_BLOCK  8
#define BYTES_PER_BLOCK         (INSTRUCTIONS_PER_BLOCK * BYTES_PER_INSTRUCTION)
#define NUMBER_OF_BLOCKS        16

#define MEM_BYTES (BYTES_PER_INSTRUCTION * INSTRUCTIONS_PER_BLOCK * NUMBER_OF_BLOCKS)


static byte program_memory[MEM_BYTES];
static byte program_memory_meta[MEM_BYTES];
//static byte program_block_flags[NUMBER_OF_BLOCKS];


static byte program_memory_updated = 0;

struct player_state {
#define PLAYER_STATE_BLOWNUP 2
#define PLAYER_STATE_ACTIVE 1
#define PLAYER_STATE_INACTIVE 0
  byte state;
  byte current_block;
  unsigned int  score;
  byte count;
  
  byte x, y;
  sbyte dx, dy;
};

struct enemy {
  byte state;
  byte type;
  
  
  byte x, y;
  sbyte dx, dy;
};

#define MAX_ENEMIES (4)
struct enemy enemies[MAX_ENEMIES];

static struct player_state players[2];

#define GAME_STATE_INTRO    0
#define GAME_STATE_GAME     1
#define GAME_STATE_GAMEOVER 2

static byte game_state = 1;

struct cpu_regs {
  byte a, x, y, pc;
  byte prev_owner;
};

static struct cpu_regs cpu_threads[2];

static byte lfsr = 0x55;

byte get_random_byte(byte rounds)
{ 
  byte out;
  while (rounds--) {
    lfsr = (lfsr >> 1) | 
      ((((lfsr >> 7) ^ (lfsr >> 5) ^ (lfsr >> 4) ^ (lfsr >> 3)) & 1) << 7);
    out = (out << 1) | (lfsr & 1);
  }
  
  return out;
}

/// SOUND EFFECTS
void sfx_cpu_tick_snare()
{
  APU_NOISE_DECAY(10, 4, 4);
}

void sfx_cpu_tick_kick()
{
  APU_NOISE_DECAY(100, 0, 0);
}

void sfx_cursor_destroy()
{
  APU_NOISE_DECAY(10, 40, 60);
  APU_PULSE_DECAY(0, 512, DUTY_12, 2, 50);
}

void sfx_value_change()
{
  APU_PULSE_DECAY(0, 256, DUTY_25, 2, 25);
}

void sfx_select() 
{
  APU_PULSE_DECAY(0, 128, DUTY_50, 2, 12);
}

void sfx_score_up()
{
  APU_PULSE_DECAY(0, 400, DUTY_50, 2, 50);
}


void __fastcall__ score_up(byte player, int inc)
{
  sfx_score_up();
  players[player].score += inc;
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
  byte i;
  memfill(program_memory, 0, MEM_BYTES);
  //memfill(program_block_flags, 0, NUMBER_OF_BLOCKS);
  memfill(program_memory_meta, 0, MEM_BYTES);
  
  players[0].state = 1;
  players[1].state = 1;
  players[0].current_block = get_random_byte(4);
  players[1].current_block = get_random_byte(4);
  players[0].score = 0x0;
  players[1].score = 0x0;
  
  players[0].x = players[0].y = 40;
  players[1].x = players[1].y = 80;
  
  players[0].dx = players[0].dy = 0;
  players[1].dx = players[1].dy = 0;
    
  memfill(&cpu_threads[0], 0, sizeof(struct cpu_regs));
  memfill(&cpu_threads[1], 0, sizeof(struct cpu_regs));
  
  cpu_threads[0].pc = players[0].current_block * BYTES_PER_BLOCK;
  cpu_threads[1].pc = players[1].current_block * BYTES_PER_BLOCK;
  
  for (i = 0 ; i < MAX_ENEMIES; i++) {
    enemies[i].dx = (i & 1) ? 1 : -1;
    enemies[i].dy = (i & 1) ? 2 : -1;
    enemies[i].x = get_random_byte(8);
    enemies[i].y = get_random_byte(8);
    enemies[i].state = 1;
  }
}

void cpu_mem_write(byte own, byte addr, byte val)
{
  if (own != 3) 
    score_up(own-1, PLAYER_SCORE_MEM_WRITE);
  
  program_memory[addr] = val;
  program_memory_meta[addr] = own;
  
  program_memory_updated = 1;
}

void cpu_tick(byte thread)
{
  struct cpu_regs *t = &cpu_threads[thread];
  // shave off 1 bit, PC accesses should always be even (nice try)
  byte pc     = t->pc &  0xFE;
  byte opcode = program_memory[pc];
  byte arg    = program_memory[pc + 1];
  byte owner  = program_memory_meta[pc];
  byte pc_mod = 0;
  
  if (t->prev_owner != owner) {
    score_up(owner-1, PLAYER_SCORE_CPU_TAKEOVER);
  }
  
  t->prev_owner = owner;
  
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
#define OPCODE_RND 9
  case OPCODE_RND:
    t->a = get_random_byte(8);
    break;
#define OPCODE_TAP 0xA
  case OPCODE_TAP:
    t->pc = t->a;
    pc_mod = 1;
    break;
#define OPCODE_RSH 0xB
  case OPCODE_RSH:
    t->a = (t->a >> arg);
    break;
#define OPCODE_LSH 0xC
  case OPCODE_LSH:
    t->a = (t->a << arg);
    break;
#define OPCODE_INCA 0xD
  case OPCODE_INCA:
    t->a = (t->a + arg);
    break;
#define OPCODE_TAX 0xE
  case OPCODE_TAX:
    t->x = t->a;
    break;
#define OPCODE_TAY 0xF
  case OPCODE_TAY:
    t->y = t->a;
    break;
#define OPCODE_STAX 0x10
  case OPCODE_STAX:
    cpu_mem_write(owner, t->x, t->a);
    break;
  }
  if (pc_mod == 0) {
    t->pc += 2;
  }
  else {
    // don't allow PCs to go to odd addresses;
    t->pc = t->pc & 0xFE;
  }
}




// muzakery
void __fastcall__ play_music(void)
{
  static const int note_table_49[64] =
    {
     4304, 4062, 3834, 3619, 
     3416, 3224, 3043, 2872, 
     2711, 2559, 2415, 2279, 
     2151, 2031, 1917, 1809, 
     1707, 1611, 1521, 1436, 
     1355, 1279, 1207, 1139, 
     1075, 1015, 958, 904, 
     853, 805, 760, 717, 
     677, 639, 603, 569, 
     537, 507, 478, 451, 
     426, 402, 379, 358, 
     338, 319, 301, 284, 
     268, 253, 239, 225, 
     213, 201, 189, 179, 
     168, 159, 150, 142, 
     134, 126, 119, 112, 
    };
  
  static byte m_ptr = 0;
  static byte m_delay = 0;
  
  int note = note_table_49[get_random_byte(5)];
  
  if (note < 800) {
    APU_PULSE_DECAY(1, note, DUTY_25, 5, 8);
  }
  else {
    APU_TRIANGLE_LENGTH(note, 1);
  }
  
  m_ptr++;
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
  byte by1 = 0;
  byte by2 = 0;
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


// draw functions use this temporarily
static byte C_BUF[10];


void draw_mem(byte sx, byte sy, struct player_state *p)
{
  byte i = 0;
  byte addr = 0;
  byte opcode = 0;
  byte arg = 0;
  byte owner = 0;
      
  for (i = 0; i < INSTRUCTIONS_PER_BLOCK * 2 ; i++) 
    {
      addr =   (p->current_block * BYTES_PER_BLOCK) + (i * 2);
      opcode = program_memory[addr];
      arg    = program_memory[(addr + 1) & 0xFF];
      owner  = program_memory_meta[addr];
    
      if (cpu_threads[0].pc == addr || cpu_threads[1].pc == addr) {
      	owner = 4;
      }
   
    
      C_BUF[0] = 1 + (addr >> 4);
      C_BUF[1] = 1 + (addr & 0xF);
      C_BUF[2] = 0x11 + owner;
      C_BUF[3] = 1 + (opcode >> 4);
      C_BUF[4] = 1 + (opcode & 0xF);
      C_BUF[5] = 0x3A;
      C_BUF[6] = 1 + (arg >> 4);
      C_BUF[7] = 1 + (arg & 0xF); 
  
      vrambuf_put(NTADR_A(sx, sy+i), C_BUF, 8);
    
      if (i == 7) {
    	ppu_wait_frame();
      	vrambuf_clear();
      }
    }
  
  ppu_wait_frame();
  vrambuf_clear();
}

void draw_cpu_thread(byte sx, byte sy, struct cpu_regs *regs)
{  
  C_BUF[0] = 'A';
  C_BUF[1] = 1 + (regs->a >> 4);
  C_BUF[2] = 1 + (regs->a & 0xF);
  C_BUF[3] = 0;
  C_BUF[4] = 'X';
  C_BUF[5] = 1 + (regs->x >> 4);
  C_BUF[6] = 1 + (regs->x & 0xF);
  C_BUF[7] = 0;
  
  vrambuf_put(NTADR_A(sx, sy+1), C_BUF, 8);

  C_BUF[0] = 'Y';
  C_BUF[1] = 1 + (regs->y >> 4);
  C_BUF[2] = 1 + (regs->y & 0xF);
  C_BUF[3] = 0;
  C_BUF[4] = 'P';
  C_BUF[5] = 1 + (regs->pc >> 4);
  C_BUF[6] = 1 + (regs->pc & 0xF);
  C_BUF[7] = 0x11 + (program_memory_meta[regs->pc & 0xFE]);

  vrambuf_put(NTADR_A(sx, sy+2), C_BUF, 8);

  ppu_wait_frame();
  vrambuf_clear();
  
}

//void draw_blocks();
//void draw_player_sprite(struct player_state *p);

#define BETWEEN(var, min, max) ((var) > (min) && (var) < (max))

#define MOVEMENT_DELTA (1)
void handle_player_input()
{
  byte i, pad;
  byte x, y;
  byte mem_x_offset = 0;
  byte opponent = 1;
  for (i=0; i<2; i++) {
    if (players[i].state == PLAYER_STATE_ACTIVE) {
      opponent = opponent - i;  
      
      x = players[i].x;
      y = players[i].y;
      // neslib says check triggers first    
      pad = pad_poll(i);
    
      if (pad & (PAD_A | PAD_B)) {
	// add some entropy to the LFSR.
      	lfsr = lfsr | 1;
      
	// did they press on a CPU?
	if (BETWEEN(x, 0, 9*8) &&
	    BETWEEN(y, 4*8, 7*8)) {
	  players[i].current_block = cpu_threads[0].pc >> 4;
	} 
      
	if (BETWEEN(x, 23*8, 31*8) &&
	    BETWEEN(y, 4*8, 7*8)) {
	  players[i].current_block = cpu_threads[1].pc >> 4;
	} 
      
	// for editing memory, each player can only modify their side
	// so we offset the x bound check when it's player 2 (1)
	if (i == 1) {
	  mem_x_offset = 23;
	}
      
	if (BETWEEN(x, ((3 + mem_x_offset) *8), (9 + mem_x_offset) * 8) &&
	    BETWEEN(y, 7*8, 24*8)) {
      	  byte addr = players[i].current_block * BYTES_PER_BLOCK;
        
          // find the row
          addr = addr + (((y - (8 * 8)) / 8) * 2);
          
	  if ((i == 0 && x > 0x30) || (i == 1 && x > 0xe0)) {
	    addr++;
          }
                  
          if (pad & PAD_B) {
	    program_memory[addr] = 0;
          }
          else {
	    if (program_memory_meta[addr] != (1 + i)) {
	      score_up(i, PLAYER_SCORE_CURSOR_MEMEDIT);
	    }
	    program_memory_meta[addr] = 1+i;

	    if (pad & PAD_UP) {
	      program_memory[addr]++;
	      sfx_value_change();

	    }
	    else if (pad & PAD_DOWN) {
	      program_memory[addr]--;
	      sfx_value_change();

	    }
	    else if (pad & PAD_LEFT) {
	      program_memory[addr] <<= 1;
	      sfx_value_change();

	    }
	    else if (pad & PAD_RIGHT) {
	      program_memory[addr] >>= 1;
	      sfx_value_change();

	    }
          }
	}
      
	if (BETWEEN(x, 0x56, 0xB2) &&
	    BETWEEN(y, 0x2E, 0x8F)) {
	  players[i].current_block = 
	    ((y - 0x2E) / 24) * 4 + 
	    ((x - 0x56) / 24);
	  sfx_select();
	}
      
	if (pad & PAD_B &&
	    (players[i].x - players[opponent].x) < 9 &&
	    (players[i].y - players[opponent].y) < 9 &&
	    players[opponent].state == PLAYER_STATE_ACTIVE) {
	  players[i].score += PLAYER_SCORE_CURSOR_DESTROY;
	  players[opponent].state = PLAYER_STATE_BLOWNUP;
	  players[opponent].count = get_random_byte(8);
	  sfx_cursor_destroy();
	}
      
	program_memory_updated = 1;
      }
      else {
    	if (pad & PAD_LEFT) players[i].dx = -MOVEMENT_DELTA;
    	else if (pad & PAD_RIGHT) players[i].dx = MOVEMENT_DELTA;
    	else players[i].dx=0;
    
    	if (pad & PAD_UP) players[i].dy = -MOVEMENT_DELTA;
    	else if (pad & PAD_DOWN) players[i].dy = MOVEMENT_DELTA;
    	else players[i].dy=0;
      }
    }
  }
}
//void handle_enemies();

void handle_sprites()
{
  byte oam_id = 0;
  byte i; 

  // move applicable sprites
  players[0].x += players[0].dx;
  players[0].y += players[0].dy;
  players[1].x += players[1].dx;
  players[1].y += players[1].dy;
  
  for (i = 0 ; i < MAX_ENEMIES ; i++) {
    if (enemies[i].state == 1) {
      enemies[i].x += enemies[i].dx;
      enemies[i].y += enemies[i].dy;
      oam_id = oam_spr(enemies[i].x, enemies[i].y, 0x17 + i, 3, oam_id);
    }
  }
  
  // draw them
  if (players[0].state == 1) {
    oam_id = oam_spr(players[0].x, players[0].y, 0x90, 0, oam_id);
  }
  if (players[1].state == 1) {
    oam_id = oam_spr(players[1].x, players[1].y, 0x91, 0, oam_id);
  }

  if (oam_id!=0) oam_hide_rest(oam_id);
}

#define GAME_LOOPS_PER_TICK 48

static byte redraw_cpu = 0;

void __fastcall__ maybe_cpu_tick(void)
{
  static byte frame_count;
  static byte flip_flop;
  
  handle_sprites();

  if (game_state != 1) {
    return;
  }
  
  if (frame_count == GAME_LOOPS_PER_TICK/2) {
    play_music();
  }
    
  if (frame_count > GAME_LOOPS_PER_TICK) {
    if ((flip_flop++ & 1) == 1) {
      cpu_tick(0);
      sfx_cpu_tick_kick();
    }
    else {
      cpu_tick(1);
      sfx_cpu_tick_snare();
    }
    frame_count = 0;
    redraw_cpu = 1;
    play_music();
  }
  
  
  frame_count++;
}

void draw_status(void)
{
  memfill(C_BUF, 0, 10);
  itoa(players[0].score, C_BUF, 16);
  
  vrambuf_put(NTADR_A(19, 1), C_BUF, 8);

  memfill(C_BUF, 0, 10);
  itoa(players[1].score, C_BUF, 16);
  
  vrambuf_put(NTADR_A(19, 2), C_BUF, 8);
  
  ppu_wait_frame();
  vrambuf_clear();
}

const char bg_row[32] = {
			 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
			 0x8D, 
			 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
			 0x8D, 
			 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0, 0x00, 0x00, 
};

void draw_gameloop_bg()
{
  byte i, x, y;
  ppu_off();
  vram_adr(0x2000);
  vram_fill(0, 960);
  
  vram_adr(NTADR_A(9, 1));
  vram_write("P1 SCORE:", 9);
  vram_adr(NTADR_A(9, 2));
  vram_write("P2 SCORE:", 9);
  
  vram_adr(NTADR_A(1, 3));
  vram_fill(0x8F, 30);
  vram_adr(NTADR_A(9, 3));
  vram_fill(0x8E, 1);
  vram_adr(NTADR_A(22, 3));
  vram_fill(0x8E, 1);

  
  
  for (i = 4; i < 28; i++) {
    vram_adr(NTADR_A(0, i));
    vram_write(bg_row, 31);
  }
  
  vram_adr(NTADR_A(1, 4));
  vram_write("CPU-----", 8);
  
  vram_adr(NTADR_A(23, 4));
  vram_write("CPU-----", 8);
  
  vram_adr(NTADR_A(1, 7));
  vram_write("MEM-----", 8);
  
  vram_adr(NTADR_A(23, 7));
  vram_write("MEM-----", 8);
  
  vram_adr(NTADR_A(1, 24));
  vram_write("OPCODE--", 8);
  
  vram_adr(NTADR_A(23, 24));
  vram_write("OPCODE--", 8);
  
  vram_adr(NTADR_A(10, 4));
  vram_write("-MEM BLOCKS-", 12);
  
  for (x = 0 ; x < 4 ; x++) {
    for (y = 0; y < 4; y++) {
      vram_adr(NTADR_A(11 + (x * 3), 6 + (y * 3)));
      vram_fill(1 + (y * 4) + x, 1);
    }
  }
  
  vram_adr(0x0);
  ppu_on_all();
}

void handle_enemies()
{
  byte i;
  struct enemy *e;
  
  for (i = 0 ; i < MAX_ENEMIES ; i++) {
    e = &enemies[i];
    	
    if (e->state == 1) {
      if (e->x < 0x4d) {
	e->dx = i+1;
      }
      else if (e->x > 0xa9) {
	e->dx = -1;
      }
          
      if (e->y < 29) {
	e->dy = i+1;
      }
      else if (e->y > 210) {
	e->dy = -1;
      }
    }
  }
}

void game_loop(void) 
{
  byte t = 0;

  draw_gameloop_bg();
  
  reset_memory();
  
  // clear vram buffer
  vrambuf_clear();
  
  // set NMI handler
  set_vram_update(updbuf);
  
  // tick in NMI as well
  nmi_set_callback(maybe_cpu_tick);
    
  draw_mem(1,  8, &players[0]);
  draw_mem(23, 8, &players[1]);

  draw_cpu_thread(1,  4, &cpu_threads[0]);
  draw_cpu_thread(23, 4, &cpu_threads[1]);
  

  while (1) 
    {
      if (redraw_cpu) {
        redraw_cpu = 0;
        draw_cpu_thread(1,  4, &cpu_threads[0]);
        draw_cpu_thread(23, 4, &cpu_threads[1]);
        draw_mem(1,  8, &players[0]);
	draw_mem(23, 8, &players[1]);
	program_memory_updated = 0;
      }
      
      if (program_memory_updated) {
	draw_mem(1,  8, &players[0]);
	draw_mem(23, 8, &players[1]);
	program_memory_updated = 0;
      }

      handle_player_input();
      handle_enemies();
    
      draw_status();
    
      for (t = 0; t < 2; t++) {
      	if (players[t].state == PLAYER_STATE_BLOWNUP) {
	  players[t].count--;
	  if (players[t].count == 0) {
	    players[t].state = PLAYER_STATE_ACTIVE;
	  }
        }
      }
    
      APU_ENABLE(ENABLE_NOISE|ENABLE_PULSE0|ENABLE_PULSE1|ENABLE_TRIANGLE);
      ppu_wait_frame();
    }
}

void main(void)
{  
  byte foo = 0;

  setup_graphics();

  ppu_on_all(); 
  
  apu_init();
  
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
