#define PLAYER_SCORE_CPU_BOMB       (64)
#define PLAYER_SCORE_CPU_TAKEOVER   (32)
#define PLAYER_SCORE_MEM_WRITE      (4)
#define PLAYER_SCORE_CURSOR_DESTROY (2)
#define PLAYER_SCORE_CURSOR_MEMEDIT (1)


#define BYTES_PER_INSTRUCTION	2
#define INSTRUCTIONS_PER_BLOCK  8
#define BYTES_PER_BLOCK         (INSTRUCTIONS_PER_BLOCK * BYTES_PER_INSTRUCTION)
#define NUMBER_OF_BLOCKS        16
#define MEM_BYTES               (BYTES_PER_INSTRUCTION * INSTRUCTIONS_PER_BLOCK * NUMBER_OF_BLOCKS)


#define PLAYER_STATE_INACTIVE   0
#define PLAYER_STATE_ACTIVE     1
#define PLAYER_STATE_BLOWNUP    2
#define PLAYER_STATE_AI         3

#define ENEMY_STATE_INACTIVE 0
#define ENEMY_STATE_ACTIVE   1
#define ENEMY_STATE_BLOWUP   2


#define MAX_ENEMIES        (1)
#define GAME_STATE_INTRO    0
#define GAME_STATE_GAME     1
#define GAME_STATE_GAMEOVER 2
#define GAME_STATE_PAUSED   3

#define GAME_MODE_SINGLE    0
#define GAME_MODE_DUAL      1

#define GAME_VICTORY_STYLE  (get_random_byte(1) & 1)


// regular instructions
#define OPCODE_NOP  0x00
#define OPCODE_LDA  0x01
#define OPCODE_LDX  0x02
#define OPCODE_TAX  0x03
#define OPCODE_TAY  0x04
#define OPCODE_STAX 0x05
#define OPCODE_RDX  0x06
#define OPCODE_INCX 0x07
#define OPCODE_INCA 0x08
#define OPCODE_HOP  0x09
#define OPCODE_JMP  0x0A
#define OPCODE_ZHOP 0x0B
#define OPCODE_WLD  0x0C
#define OPCODE_WCP  0x0D
#define OPCODE_MEMW 0x0E
#define OPCODE_RND  0x0F 
#define OPCODE_TAP  0x10
#define OPCODE_RSH  0x11
#define OPCODE_LSH  0x12
#define OPCODE_XYS  0x13 // -- swap X<->Y
#define OPCODE_AXS  0x14
#define OPCODE_CAX  0x15 // compare mem[x] == a, skip eq
#define OPCODE_AND  0x16
#define OPCODE_TPX  0x17
#define OPCODE_TXP  0x18


// stupid insrtructions
#define OPCODE_RCP  0x42
#define OPCODE_LDW  0x3D


#define AI_N_PROGRAMS 3

static const unsigned char ai_programs[] =
  {
   // PROGRAM 1
   OPCODE_LDA,  0x00,
   OPCODE_TAX,  0x00,
   OPCODE_RND,  0x00,
   OPCODE_STAX, 0x00,
   OPCODE_INCX, 0x01,
   OPCODE_HOP,  0xFB,
   OPCODE_NOP,  0x0,
   OPCODE_NOP,  0x0,
   // PROGRAM 2
   OPCODE_LDA,  0x02,
   OPCODE_TAX,  0x00,
   OPCODE_RCP,  0x00,
   OPCODE_NOP,  0x00,
   OPCODE_NOP,  0xDB,
   OPCODE_HOP,  0xEE,
   OPCODE_NOP,  0xAE,
   OPCODE_NOP,  0xDF,
   // PROGRAM 3
   OPCODE_RND,   0x02,
   OPCODE_TAX,   0x00,
   OPCODE_RDX,   0x00,
   OPCODE_MEMW,  0x00,
   OPCODE_RND,   0xDB,
   OPCODE_STAX,  0xEE,
   OPCODE_HOP,   0xF2,
   OPCODE_NOP,   0xDF,
     // PROGRAM 4
   OPCODE_NOP,   0x02,
   OPCODE_NOP,   0x00,
   OPCODE_NOP,   0x00,
   OPCODE_NOP,   0x00,
   OPCODE_NOP,   0xDB,
   OPCODE_NOP,   0xEE,
   OPCODE_NOP,   0xAE,
   OPCODE_NOP,   0xDF,
  };



#define BETWEEN(var, min, max) ((var) > (min) && (var) < (max))
#define MOVEMENT_DELTA (1)
#define GAME_LOOPS_PER_TICK 32

