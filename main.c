#include <stdlib.h>
#include <string.h>

#include "main.h"

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
   
   0x21,0x00,0x30,0x00,	// background palette 0
   0x1C,0x20,0x2C,0x00,	// background palette 1
   0x00,0x10,0x20,0x00,	// background palette 2
   0x06,0x16,0x26,0x00,   // background palette 3

   0x16,0x35,0x24,0x00,	// sprite palette 0
   0x00,0x37,0x25,0x00,	// sprite palette 1
   0x0D,0x2D,0x15,0x00,	// sprite palette 2
   0x0D,0x00,0x27	// sprite palette 3
  };

static byte program_memory[MEM_BYTES];
static byte program_memory_meta[MEM_BYTES];
//static byte program_block_flags[NUMBER_OF_BLOCKS];

static byte program_memory_touched = 0;
static byte program_memory_updated = 0;

struct player_state {
  byte state;
  byte current_block;
  unsigned int  score;
  byte count;
  
  byte x, y;
  sbyte dx, dy;
};

struct enemy {
  byte state;
  byte count_addr;
  byte type;
  
  
  byte x, y;
  sbyte dx, dy;
};

struct enemy enemies[MAX_ENEMIES];

static struct player_state players[2];


static byte	game_state	   = 0;
static byte	game_mode	   = 0;
static byte	game_victory_style = 0;
static byte	watchdog	   = 255;

struct cpu_regs {
  byte a, x, y, pc;
  byte prev_owner;
};

static struct cpu_regs cpu_threads[2];



static byte lfsr = 0x55;

byte get_random_byte(byte rounds)
{ 
  byte out = 0;
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
  APU_NOISE_DECAY(10, 4, 1);
}

void sfx_cpu_tick_kick()
{
  APU_NOISE_DECAY(100, 1, 2);
}

void sfx_cursor_destroy()
{
  APU_NOISE_DECAY(10, 40, 60);
  APU_PULSE_DECAY(0, 512, DUTY_12, 2, 50);
}

void sfx_enemy_detonate()
{
  APU_NOISE_DECAY(10, 40, 60);
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
  APU_PULSE_DECAY(0, 200, DUTY_25, 1, 10);
}


void __fastcall__ score_up(byte player, int inc)
{
  sfx_score_up();
  players[player].score += inc;
}

// setup PPU and tables
void setup_graphics()
{
  // clear sprites
  oam_clear();
  // set palette colors
  pal_all(PALETTE);
}


static byte free_memory_count_last = 255;

void update_free_memory_count()
{
  byte i = 0xFF;
  byte c = 0;
  
  while (i > 0) {
    if (program_memory_meta[i] == 0) {
      c++;
    }
    i--;
  }
  
  free_memory_count_last = c;
}

void update_memory_ownership(byte addr, byte owner)
{
  program_memory_meta[addr] = owner;
}

void cpu_mem_write(byte own, byte addr, byte val)
{
  if (own != 3) 
    score_up(own-1, PLAYER_SCORE_MEM_WRITE);
  
  program_memory[addr] = val;
  
  update_memory_ownership(addr, own);
  
  program_memory_updated = 1;
}


static byte last_program_location = 0;
static byte last_program_number = 0;

void ai_place_program(byte force)
{
  byte i;
  byte violated = force;

  // check if previous program has been violated in some fashion. 
  // if it has, abandon it and move elsewhere
  if (! force) {
    for (i = 0 ; i < 16; i++) {
      if (program_memory[last_program_location + i] != 
	  ai_programs[(last_program_number * 16) + i]) {
	violated = 1;
      }
     
      // return memory to system
      if (violated) {
	for (i = 0 ; i < 16; i++) {
	  update_memory_ownership(last_program_location + i, 0);
	}
      }
    }
  }
  
  if (violated) {
    last_program_location = get_random_byte(7) << 1;
    last_program_number   = get_random_byte(2);
    
    for (i = 0 ; i < 16 ; i++) {
      cpu_mem_write(2, last_program_location + i, 
		    ai_programs[(last_program_number * 16) + i]);
    }
    
    // HIJACK CPU1
    cpu_threads[1].pc = last_program_location;
    cpu_threads[1].prev_owner = 2;
    players[1].current_block = last_program_location >> 4;
  }
}

void reset_memory()
{
  byte i;
  memfill(program_memory, 0, MEM_BYTES);
  //memfill(program_block_flags, 0, NUMBER_OF_BLOCKS);
  memfill(program_memory_meta, 0, MEM_BYTES);
  program_memory_touched = 0;
  
  players[0].state = PLAYER_STATE_ACTIVE;
  players[1].state = PLAYER_STATE_ACTIVE;
  
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
  
  if (game_mode == GAME_MODE_SINGLE) {
    players[1].state = PLAYER_STATE_AI;
    program_memory[0] = get_random_byte(5);
  }
    
  for (i = 0 ; i < MAX_ENEMIES; i++) {
    enemies[i].dx = 0;
    enemies[i].dy = 0;
    enemies[i].x = get_random_byte(8);
    enemies[i].y = get_random_byte(8);
    enemies[i].count_addr = get_random_byte(8);
    enemies[i].state = ENEMY_STATE_INACTIVE;
  }
  
  game_victory_style = GAME_VICTORY_STYLE;
  
  free_memory_count_last = 255;
  watchdog = 255;
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
  byte tmp = 0;
  
  if (t->prev_owner != owner && 
      (owner == 1 && owner == 2)) {
    score_up(owner-1, PLAYER_SCORE_CPU_TAKEOVER);
  }
  
  switch (opcode) {
  case OPCODE_NOP:
    break;
  case OPCODE_LDA:
    t->a = arg;
    break;
  case OPCODE_LDX:
    t->x = arg;
    break;
  case OPCODE_HOP:
    pc_mod = 1;
    t->pc += (signed char) arg;
    break;
  case OPCODE_JMP:
    t->pc = arg;
    pc_mod = 1;
    break;
  case OPCODE_DECY:
    t->y--;
    if (t->y == 0) {
      pc_mod = 1;
      t->pc += 4;
    }
    break;
  case OPCODE_WLD:
    t->y = program_memory[(t->x + 1) & 0xFF];
    t->a = program_memory[t->x];
    break;
  case OPCODE_WCP:
    cpu_mem_write(owner, t->x, t->a);
    cpu_mem_write(owner, t->x + 1, t->y);
    break;
  case OPCODE_MEMW:
    pc_mod = 1;
    if (program_memory[t->x] != t->a) {
      t->pc += 2;
    }
    break;
  case OPCODE_RND:
    t->a = get_random_byte(8);
    break;
  case OPCODE_STXX:
    cpu_mem_write(owner, program_memory[t->x], t->a);
    break;
  case OPCODE_RSH:
    t->a = (t->a >> arg);
    break;
  case OPCODE_LSH:
    t->a = (t->a << arg);
    break;
  case OPCODE_INCA:
    t->a = (t->a + (sbyte) arg);
    break;
  case OPCODE_TAX:
    t->x = t->a;
    break;
  case OPCODE_TAY:
    t->y = t->a;
    break;
  case OPCODE_STAX:
    cpu_mem_write(owner, t->x, t->a);
    break;
  case OPCODE_INCX:
    t->x += arg;
    break;
  case OPCODE_RDX:
    t->a = program_memory[t->x];
    break;
  case OPCODE_RCP:
    // stupidly OP instruction, but here so we can implement IMP like.
    cpu_mem_write(owner, pc + t->x,   program_memory[pc + ((sbyte) arg)]);
    cpu_mem_write(owner, pc + t->x+1, program_memory[pc + ((sbyte) arg)+1]);
    break;
  case OPCODE_XYS:
    tmp = t->x;
    t->x = t->y;
    t->y = tmp;
    break;
  case OPCODE_AXS:
    tmp = t->x;
    t->x = t->a;
    t->a = tmp;
    break;
  case OPCODE_CAX:
    if (program_memory[t->x] == t->a) {
      pc_mod = 1;
      t->pc += 0x4;
    }
    break;
  case OPCODE_LDW:
    watchdog = get_random_byte(8);
    break;
  case OPCODE_AND:
    t->a = t->a & arg;
    break;
  case OPCODE_TPX:
    t->x = t->pc;
    break;
  case OPCODE_TXP:
    t->pc = t->x;
    break;
  case OPCODE_DLY:
    pc_mod = 1;
    if (t->a == 0) {
      pc += 2;
    }
    else {
      t->a--;
    }
    break;
  default:
    pc_mod = 1;
    t->pc = 0x2;
    
    if ((owner == 1 || owner == 2) && 
        (t->prev_owner == 1 || t->prev_owner == 2) &&
        owner != t->prev_owner) {
      score_up(owner-1, PLAYER_SCORE_CPU_BOMB);
    }
  }
  
  t->prev_owner = owner;
  
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
  static const int bass_notes[] =
    { 
     0x17c,
     0x13f,
     0x17c,
     0x0fd,
     0x17c,
     0x0d2,
     0x17c,
     0x13f,
     0x17c,
     0x0fd,
     0x17c,
     0x0d2,
     0x17c,
     0x13f,
     0x17c,
     0x1ab,
    };
  
  static const int treb_notes[] =
    { 
     // measure 1
     0x17c,
     0x17c,
     0x11c,
     0x13f,
     0x152,
     0x17c,
     0x17c,
     0x13f,
     0x152,
     0x13f,
     0x0fd,
     0x0e2,
     0x11c,
     0x13f,
     0x1ab,
     0x193,
    
     // measure 2
     0x17c,
     0x152,
     0x13f,
     0x152,
     0x11c,
     0x152,
     0x13f,
     0x152,
     0x0fd,
     0x152,
     0x0e2,
     0x0fd,
     0x0e2,
     0x13f,
     0x1ab,
     0x193,
    };
  
  static byte m_ptr = 0;
  static byte m_delay = 0;
    

  APU_TRIANGLE_LENGTH(bass_notes[m_ptr & 0xF], 2);
  
  
  
  APU_PULSE_DECAY(
		  1,
		  treb_notes[m_ptr & 0x1F], 
		  DUTY_75,
		  2, 
		  10);
  
  
  APU_PULSE_SET_VOLUME(1, DUTY_75, 3);
  
  m_ptr++;
}

void stop_music()
{
  APU_TRIANGLE_LENGTH(0, 0);
  APU_PULSE_DECAY(1, 200, DUTY_25, 1, 10);
}

void clrscr()
{
  ppu_off();
  vram_adr(0x2000);
  vram_fill(0, 32*28);
  vram_adr(0x0);
  ppu_on_all();
}

byte title_screen(void)
{
  byte by1 = 0;
  byte by2 = 0;
  byte mode = 0;
  byte oam_id = 0;
  clrscr();
  
  vram_adr(NTADR_A(9,8));
  vram_write("ARCASM", 6);
  
  vram_adr(NTADR_A(11, 12));
  vram_write("VS. CPU MODE", 13);
    
  vram_adr(NTADR_A(11, 16));
  vram_write("DUEL", 4);
      
  ppu_wait_frame();
  
  vram_adr(NTADR_A(2, 26));
  vram_write("> Z6580 CPU SYSTEM", 18);
    
  while (1) {
    get_random_byte(1);
    by2 = pad_trigger(0) | pad_trigger(1);
    if (by2 & (PAD_UP | PAD_DOWN | PAD_SELECT)) {
      if (mode) { mode = 0; }
      else { mode = 1; }
    }
    
    if (mode) {
      oam_id = oam_spr(72, 128, 0x1F, 1, oam_id);
    } 
    else {
      oam_id = oam_spr(72, 95, 0x1F, 1, oam_id);
    }
    oam_hide_rest(oam_id);  
    if (by2 & PAD_START) break;
  }
  
  
  return mode;
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
    if ((pad_trigger(0) | pad_trigger(1)) & PAD_START) break;
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


void handle_player_input()
{
  byte i, pad;
  byte x, y;
  byte mem_x_offset = 0;
  byte opponent = 1;
  
  pad = pad_trigger(0) | pad_trigger(1);
  
  if (pad & PAD_START) {
    if (game_state == GAME_STATE_GAME) {
      game_state = GAME_STATE_PAUSED;
      stop_music();
    }
    else if (game_state == GAME_STATE_PAUSED) {
      game_state = GAME_STATE_GAME;
    }
  }
  
  for (i=0; i<2; i++) {
    pad = pad_poll(i);
    
    if ((pad & (PAD_START | PAD_SELECT)) == (PAD_START | PAD_SELECT)) {
      game_state = GAME_STATE_INTRO;
      return;
    }
    

    
    if (players[i].state == PLAYER_STATE_ACTIVE) {
      opponent = opponent - i;  
      
      x = players[i].x;
      y = players[i].y;
      // neslib says check triggers first    
      

      
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
	  else if (pad & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)) {
	    if (program_memory_meta[addr] != (1 + i)) {
	      score_up(i, PLAYER_SCORE_CURSOR_MEMEDIT);
	      update_memory_ownership(addr, 1+i);
	    }

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

void handle_sprites()
{
  byte oam_id = 0;
  byte i; 
  byte sprite_id;

  // move applicable sprites
  players[0].x += players[0].dx;
  players[0].y += players[0].dy;
  players[1].x += players[1].dx;
  players[1].y += players[1].dy;
  
  // draw them
  if (players[0].state == PLAYER_STATE_ACTIVE) {
    oam_id = oam_spr(players[0].x, players[0].y, 0x90, 0, oam_id);
  }
  else if (players[0].state == PLAYER_STATE_BLOWNUP) {
    oam_id = oam_spr(players[0].x, players[0].y, 1 + (players[0].count >> 4), 0, oam_id); 
  }
  
  if (players[1].state == PLAYER_STATE_ACTIVE) {
    oam_id = oam_spr(players[1].x, players[1].y, 0x91, 0, oam_id);
  }
  else if (players[1].state == PLAYER_STATE_BLOWNUP) {
    oam_id = oam_spr(players[1].x, players[1].y, 1 + (players[1].count >> 4), 0, oam_id);
	
  }
  
  for (i = 0 ; i < MAX_ENEMIES ; i++) {
    if (enemies[i].state != ENEMY_STATE_INACTIVE) {
      enemies[i].x += enemies[i].dx;
      enemies[i].y += enemies[i].dy;
      sprite_id = (enemies[i].state == ENEMY_STATE_BLOWUP) ? 0x21 : 0x19 + i;
      oam_id = oam_spr(enemies[i].x, enemies[i].y, sprite_id, 3, oam_id);
    }
  }
  
  for (i = 0 ; i < 2 ; i++) {
    byte block = cpu_threads[i].pc >> 4;
    byte by    = 0x2F + ((block >> 2) * 24);
    byte bx    = 0x62 + ((block &  3) * 24);
    
    oam_id = oam_spr(
		     bx,
		     by,
		     0x15,
		     2+i,
		     oam_id,
		     );
  }

  if (oam_id!=0) oam_hide_rest(oam_id);
}


static byte redraw_cpu = 0;

void __fastcall__ maybe_cpu_tick(void)
{
  static byte frame_count;
  static byte flip_flop;
  
  if (game_state != 1) {
    return;
  }
  
  handle_sprites();
  
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

void draw_status()
{
  memfill(C_BUF, 0, 10);
  itoa(players[0].score, C_BUF, 16);
  vrambuf_put(NTADR_A(19, 1), C_BUF, 8);
  
  memfill(C_BUF, 0, 10);
  itoa(players[1].score, C_BUF, 16);
  vrambuf_put(NTADR_A(19, 2), C_BUF, 8);
  
  memfill(C_BUF, 0, 10);
  if (game_victory_style == 1) {
    itoa(watchdog, C_BUF, 16);
    vrambuf_put(NTADR_A(18, 26), C_BUF, 10);
  }
  else {
    itoa(free_memory_count_last, C_BUF, 16);
    vrambuf_put(NTADR_A(18, 26), C_BUF, 10);
  }
  
  ppu_wait_frame();
  vrambuf_clear();
}

void draw_gameover(void)
{

  if (game_mode == GAME_MODE_SINGLE) {
    if (players[0].score > players[1].score) {
      vrambuf_put(NTADR_A(10, 25), "YOU WIN", 7);
    }
    else {
      vrambuf_put(NTADR_A(10, 25), "YOU LOSE", 9);
    }
  }
  else {
    if (players[0].score > players[1].score) {
      vrambuf_put(NTADR_A(10, 25), "PLAYER 1 WINS", 13);
    }
    else if (players[0].score == players[1].score) {
      vrambuf_put(NTADR_A(10, 25), "YOU BOTH SUCK", 14);
    }
    else {
      vrambuf_put(NTADR_A(10, 25), "PLAYER 2 WINS", 13);
    }
  }

  
  ppu_wait_frame();
  vrambuf_clear();
  
  while (1) {
    if ((pad_trigger(0) | pad_trigger(1)) & PAD_START) break;
  }
}

const char bg_row[32] = 
  {
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
  
  vram_adr(NTADR_A(8, 26));
  if (game_victory_style == 1) {
    vram_write("WATCHDOG: ", 10);
  }
  else {
    vram_write("FREE MEM: ", 10);
  }
  
  vram_adr(NTADR_A(1, 3));
  vram_fill(0x8F, 30);
  vram_adr(NTADR_A(9, 3));
  vram_fill(0x8E, 1);
  vram_adr(NTADR_A(22, 3));
  vram_fill(0x8E, 1);

  
  
  vram_adr(NTADR_A(1, 24));
  vram_fill(0x8F, 30);
  vram_adr(NTADR_A(9, 24));
  vram_fill(0x8C, 1);
  vram_adr(NTADR_A(22, 24));
  vram_fill(0x8C, 1);
  
  
  for (i = 4; i < 24; i++) {
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

static byte enemy_delay_ctr = 0;

void handle_enemies()
{
  byte i;
  byte x;
  struct enemy *e;
  
  enemy_delay_ctr++;
  
  if (enemy_delay_ctr > 0x4) {
    program_memory[e->count_addr]--;
    program_memory_updated = 1;
    enemy_delay_ctr = 0;
  }
  
  for (i = 0 ; i < MAX_ENEMIES ; i++) {
    e = &enemies[i];
    
    
    if (e->state == ENEMY_STATE_ACTIVE) {
      if (e->x < 0x54) {
	e->dx = i+1;
      }
      else if (e->x > 0xa9) {
	e->dx = -1;
      }
          
      if (e->y < 37) {
	e->dy = i+1;
      }
      else if (e->y > 188) {
	e->dy = -1;
      }
    }
    
    if (program_memory[e->count_addr] == 0) {
      if (e->state == ENEMY_STATE_INACTIVE) {
        e->state = ENEMY_STATE_ACTIVE;
        e->dx = 1;
        e->dy = 1;
        e->x = 100;
        e->y = 100;
        program_memory[e->count_addr] = get_random_byte(8);
      }
      else if (e->state == ENEMY_STATE_BLOWUP) {
        e->state = ENEMY_STATE_INACTIVE;
        program_memory[e->count_addr] = get_random_byte(8);
      }
      else {
      	// blow up.
      	sfx_enemy_detonate();
        
        if (BETWEEN(e->x, 0x56, 0xB2) &&
	    BETWEEN(e->y, 0x2E, 0x8F)) {
	  byte current_block = 
	    ((e->y - 0x2E) / 24) * 4 + 
	    ((e->x - 0x56) / 24);
        
	  for (x = 0; x < BYTES_PER_BLOCK ; x++) {
	    byte a = (current_block * BYTES_PER_BLOCK) + x;
	    program_memory[a] = 0;
	    update_memory_ownership(a, 0);
	  }
        }
        program_memory[e->count_addr] = 5;
        e->state = ENEMY_STATE_BLOWUP;
        e->dx = 0;
        e->dy = 0;
      }
    }
  }
}

byte gameover_check()
{
  if (game_victory_style == 1) {
    if (watchdog == 0) {
      return true;
    }
  }
  else {
    if (free_memory_count_last == 0) {
      return true;
    }
  }
  
  return false;
}

void game_loop(void) 
{
  byte t = 0;
  byte c = 0;
  
  
  clrscr();
  setup_graphics();
  
  reset_memory();
  
  draw_gameloop_bg();
  
  
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
  

  while (game_state == GAME_STATE_GAME || game_state == GAME_STATE_PAUSED) 
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
        update_free_memory_count();
	program_memory_updated = 0;
      }

      handle_player_input();
    
      draw_status();
    
      if (game_state == GAME_STATE_GAME) {
	handle_enemies();

	for (t = 0; t < 2; t++) {
	  if (players[t].state == PLAYER_STATE_BLOWNUP) {
	    players[t].count--;
	    if (players[t].count == 0) {
	      players[t].state = PLAYER_STATE_ACTIVE;
	    }
	  }
	}
    
	//APU_ENABLE(ENABLE_NOISE|ENABLE_PULSE0|ENABLE_PULSE1|ENABLE_TRIANGLE);
    
	if (c > 0x10) {
	  if (game_mode == GAME_MODE_SINGLE) {
	    if (program_memory[0] == 0) {
	      ai_place_program(0);
	      program_memory[0] = get_random_byte(6);
	    }
	    else {
	      program_memory[0]--;
	      program_memory_updated = 1;
	    }
	  }
	  if (gameover_check()) {
	    game_state = GAME_STATE_GAMEOVER;
	    draw_gameover();
	    return;
	  }
	  watchdog--;
	  c = 0;
	}
	else {
	  c++; // geddit???
	}
      }
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
    switch (game_state) 
      {
      case GAME_STATE_INTRO:
        game_mode = title_screen();
        game_state = GAME_STATE_GAME;
        break;
      case GAME_STATE_GAME:
        game_loop();
        game_state = GAME_STATE_INTRO;
        break;
      }
  }
}
