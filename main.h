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
#define OPCODE_NOP  0x00 // do nothing
#define OPCODE_LDA  0x01 // load argument into A
#define OPCODE_LDX  0x02 // load argument into X
#define OPCODE_TAX  0x03 // transfer A->X
#define OPCODE_TAY  0x04 // transfer A->Y
#define OPCODE_STAX 0x05 // Store A into address *X
#define OPCODE_RDX  0x06 // Read address *X into A
#define OPCODE_INCX 0x07 // Increment X by arg
#define OPCODE_INCA 0x08 // increment A by arg
#define OPCODE_HOP  0x09 // Relative Jump (PC = PC + Arg) 
#define OPCODE_JMP  0x0A // Absolote Jump (PC = Arg)
#define OPCODE_DECY 0x0B // Decrement Y, skip next 0
#define OPCODE_WLD  0x0C // WIDE LOAD. A = *X, Y=*(X+1)
#define OPCODE_WCP  0x0D // WIDE COPY *X = A, *(X + 1) = Y
#define OPCODE_MEMW 0x0E // Mem Wait. Wait until *X != A
#define OPCODE_RND  0x0F // Load A with Random Number
#define OPCODE_STXX 0x10 // Store A at **X
#define OPCODE_RSH  0x11 // Right shift A by ARG places
#define OPCODE_LSH  0x12 // Left shift A by ARG places
#define OPCODE_XYS  0x13 // Swap X<->Y
#define OPCODE_AXS  0x14 // Swap A<->X
#define OPCODE_CAX  0x15 // compare mem[x] == a, skip eq
#define OPCODE_AND  0x16 // A = A & Arg
#define OPCODE_TPX  0x17 // Transfer P->X
#define OPCODE_TXP  0x18 // Transfer X->P
#define OPCODE_XOR  0x19 // A = A ^ Arg


// stupid insrtructions
#define OPCODE_RCP  0x42  // relative copy. copy 16bits from *(pc + x) to to *(pc + arg)
#define OPCODE_LDW  0x3D  // load the watchdog timer with a random value
#define OPCODE_DLY  0xDE  // decrement A, sleep if 0


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
   OPCODE_RND,   0x02,
   OPCODE_AND,   0xFE,
   OPCODE_TAX,   0x00,
   OPCODE_RDX,   0x00,
   OPCODE_MEMW,  0x00,
   OPCODE_STAX,  0xDB,
   OPCODE_HOP,   0xF2,
   OPCODE_NOP,   0xDF,
  };



#define BETWEEN(var, min, max) ((var) > (min) && (var) < (max))
#define MOVEMENT_DELTA (1)
#define GAME_LOOPS_PER_TICK 32

