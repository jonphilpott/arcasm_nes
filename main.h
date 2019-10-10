#define PLAYER_SCORE_CPU_TAKEOVER   (128)
#define PLAYER_SCORE_MEM_WRITE      (128)
#define PLAYER_SCORE_CURSOR_DESTROY (64)
#define PLAYER_SCORE_CURSOR_MEMEDIT (8)


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

#define GAME_MODE_SINGLE    0
#define GAME_MODE_DUAL      1
#define AI_N_PROGRAMS       8



// regular instructions
#define OPCODE_NOP  0x00
#define OPCODE_LDA  0x01
#define OPCODE_STA  0x02
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

// stupid insrtructions
#define OPCODE_RCP  0x42


static const unsigned char ai_programs[] =
  {
   OPCODE_LDA,  0x00,
   OPCODE_TAX,  0x00,
   OPCODE_RND,  0x00,
   OPCODE_STAX, 0x00,
   OPCODE_INCX, 0x01,
   OPCODE_HOP,  0xFB,
   OPCODE_NOP,  0x0,
   OPCODE_NOP,  0x0,
   // PROGRAM 1
   0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 
   // PROGRAM 2
   0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 
   // PROGRAM 3
   0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 
   // PROGRAM 4
   0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 
   // PROGRAM 5
   0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 
   // PROGRAM 6
   0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 
   // PROGRAM 7
   0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
   // PROGRAM 8
   0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 
  };



#define BETWEEN(var, min, max) ((var) > (min) && (var) < (max))
#define MOVEMENT_DELTA (1)
#define GAME_LOOPS_PER_TICK 32

