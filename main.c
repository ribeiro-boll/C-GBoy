#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
//#include <threads.h>
//#include <unistd.h>
#include <time.h>
#include <SDL2/SDL.h>
// bits    mais clara
// 00    Cor 0: #9BBC0F  VVV
// 01    Cor 1: #8BAC0F  |||
// 10    Cor 2: #306230  VVV
// 11    Cor 3: #0F380F
//   mais escura
void set_interrupt(int bit, bool true_or_false);

typedef struct Memory {
    int game_rom_lenght;
    uint8_t MBC_type;
    uint8_t *game_rom;       // read binary da ROM intira e dar memcpy para ca
    uint8_t MBC_curr_bank;
    uint8_t VRAM[0x2000];    // inicia -> 0x8000 | final ->0x9FFF
    uint8_t cartRAM[0x2000]; // inicia -> 0xA000 | final ->0xBFFF
    uint8_t WRAM[0x2000];    // inicia -> 0xC000 | final ->0xDFFF
    uint8_t OAM[0x00A0];     // inicia -> 0xFE00 | final ->0xFE9F
    uint8_t IO[0x0080];      // inicia -> 0xFF00 | final ->0xFF7F
    uint8_t HRAM[0x007F];    // inicia -> 0xFF80 | final ->0xFFFE
    uint8_t IE;              // endereço -> 0xFFFF
} GB_Memory;


typedef struct sprite {
    uint8_t coord_y;
    uint8_t coord_x;
    uint8_t tile_ID;
    uint8_t tile_ID_8x16;
    uint8_t attributes;
} Sprite;
//Sprite sprite_bank[40];

uint32_t colors[] = {0x9BBC0F,0x8BAC0F,0x306230,0x0F380F};

typedef struct Reg {
    bool is_halted;
    bool only_waiting_for_interrupt_cond;
    bool halt_bug;
    bool tima_is_on;
    bool enable_interrupt;
    bool IME;

    bool DMA_transfer_pending;
    uint16_t  DMA_transfer_OAM_addr;
    uint16_t DMA_transfer_curr_addr;
    uint16_t DMA_transfer_limit_addr;

    uint16_t SP;
    uint16_t PC;

    uint8_t A;
    uint8_t B;
    uint8_t C;
    uint8_t D;
    uint8_t E;
    uint8_t H;
    uint8_t L;

    uint8_t F; // registrador de flags, apenas usa os 4 primeiros bits ex 1011 0000

    uint16_t contador_ciclos;
    uint16_t contador_ciclos_div;
    uint16_t contador_ciclos_tima;

    uint64_t frame_start;
    uint64_t frame_end;
    double time_elapsed;
} GB_reg;

#define REG_LCDC 0xFF40 // FF40
#define REG_STAT 0xFF41 // FF41
#define REG_SCY  0xFF42 // FF42
#define REG_SCX  0xFF43 // FF43
#define REG_LY   0xFF44 // FF44
#define REG_LYC  0xFF45 // FF45
#define REG_BGP  0xFF47 // FF47
#define REG_WY   0xFF4A // FF4A
#define REG_WX   0xFF4B // FF4B
#define REG_IF   0xFF0F // FF0F

typedef struct Pixel_coords {
    uint8_t x;
    uint8_t y;
} Pixel_coords;

typedef struct GB_PPU {
    // TODO(PPU): revisar estes caches na reescrita; com renderização direta por scanline,
    // TODO(PPU): tile_data/background/window e current_y_from_tiles podem deixar de existir.
    uint8_t current_LY_from_window;
    uint8_t current_LY;
    uint8_t LCDscreen[144][160];
    uint8_t LCDscreen_pixel_color[144][160];
    Sprite sprite_bank[10];
    uint8_t sprite_count;
    // mode 0 blank horizontal
    // mode 1 vblank | blank vertical
    // mode 2 OAM scan | prep para o modo 3
    // modo 3 modo de draw dos pixels
    int current_mode;
    bool executou_modo_0;
    bool executou_modo_1;
    bool executou_modo_2;
    bool executou_modo_3;
    bool executou_ly_lyc;
    Pixel_coords window_top_left;
    uint16_t contador_ciclos;
} GB_PPU;

double emulation_delay_target = 16.74;
double emulation_delay_left = 0;

bool vblank_start_joypad = true;

bool cond_start_vblank = false;
bool cond_ja_foi_vblank = false;

bool cond_start_STAT = false;
bool cond_ja_foi_STAT = false;

bool cond_estado_3_da_ppu_foi_mudado = true;
bool cond_estado_2_da_ppu_foi_mudado = true;
bool cond_estado_1_da_ppu_foi_mudado = true;
bool cond_estado_0_da_ppu_foi_mudado = true;

bool on_left_button = false;
bool on_right_button = false;
bool on_down_button = false;
bool on_up_button = false;

bool on_A_button = false;
bool on_B_button = false;
bool on_select_button = false;
bool on_start_button = false;

GB_PPU ppu;
GB_reg cpu;
GB_Memory memory;

SDL_Window *screen = NULL;
SDL_Surface *screenSurface = NULL;
SDL_Renderer *renderer = NULL;

// ------------------ begin debug -----------
bool cond_first_run = true;
bool cond_debug = false;
int nmbr_tests = 14;
#define BASE 0x0100
#define MAX_PROG_SIZE 0x80
typedef struct {
    const char *name;
    uint16_t size;
    uint8_t code[MAX_PROG_SIZE];
} TestProgram;
// ============================================================================
// Testes extras pro emulador — cobrem o que faltava no seu conjunto original.
// Sem nenhuma instrução do prefixo CB (rotates/shifts/BIT/SET/RES em r8),
// já que você disse que essa tabela ainda não foi implementada.
//
// Todos os valores de "estado final esperado" abaixo foi calculado rodando cada
// programa num interpretador SM83 escrito à parte, então é gabarito independente, não "o que seu emu daria".
// ============================================================================
TestProgram tests_extra[] = {

    {
        "ld_hl_indirect_completo",
        0x20,
        {
            0x31, 0xFE, 0xFF, // 0100: LD SP,FFFE
            0x21, 0x00, 0xC0, // 0103: LD HL,C000

            0x3E, 0x3C,       // 0106: LD A,3C
            0x77,             // 0108: LD [HL],A     ; mem[C000]=3C

            0x46,             // 0109: LD B,[HL]     ; B=3C

            0x36, 0x7E,       // 010A: LD [HL],7E    ; mem[C000]=7E
            0x4E,             // 010C: LD C,[HL]     ; C=7E

            0x16, 0x11,       // 010D: LD D,11
            0x72,             // 010F: LD [HL],D     ; mem[C000]=11
            0x5E,             // 0110: LD E,[HL]     ; E=11

            0x71,             // 0111: LD [HL],C     ; mem[C000]=7E
            0x7E,             // 0112: LD A,[HL]     ; A=7E

            0x10, 0x00        // 0113: STOP
        }
    },
    // ESPERADO NO FINAL:
    // A=7E B=3C C=7E D=11 E=11 H=C0 L=00 | F=00 (Z=0 N=0 H=0 C=0) | SP=FFFE

    {
        "ld_indirect_wraparound",
        0x30,
        {
            0x31, 0xFE, 0xFF, // 0100: LD SP,FFFE
            0x21, 0xFF, 0xFF, // 0103: LD HL,FFFF

            0x3E, 0xAB,       // 0106: LD A,AB
            0x22,             // 0108: LD [HL+],A   ; mem[FFFF]=AB, HL vira 0000

            0x3E, 0xCD,       // 0109: LD A,CD
            0x32,             // 010B: LD [HL-],A   ; mem[0000]=CD, HL vira FFFF

            0x2A,             // 010C: LD A,[HL+]   ; A=AB (mem[FFFF]), HL vira 0000
            0x3A,             // 010D: LD A,[HL-]   ; A=CD (mem[0000]), HL vira FFFF

            0x01, 0x01, 0xC0, // 010E: LD BC,C001
            0x3E, 0x55,       // 0111: LD A,55
            0x02,             // 0113: LD [BC],A

            0x11, 0x02, 0xC0, // 0114: LD DE,C002
            0x3E, 0x66,       // 0117: LD A,66
            0x12,             // 0119: LD [DE],A

            0x0A,             // 011A: LD A,[BC]    ; A=55
            0x1A,             // 011B: LD A,[DE]    ; A=66

            0x10, 0x00        // 011C: STOP
        }
    },
    // ESPERADO NO FINAL:
    // A=66 B=C0 C=01 D=C0 E=02 H=FF L=FF | F=00 (Z=0 N=0 H=0 C=0) | SP=FFFE
    // (o importante aqui é HL dar a volta certinho 0000<->FFFF nos +/- )

    {
        "alu_bordas_flags",
        0x30,
        {
            0x3E, 0x0F,       // 0100: LD A,0F
            0xC6, 0x01,       // 0102: ADD A,01   ; A=10  H=1 C=0 Z=0
            0x3E, 0xFF,       // 0104: LD A,FF
            0xC6, 0x01,       // 0106: ADD A,01   ; A=00  Z=1 H=1 C=1
            0x3E, 0xF0,       // 0108: LD A,F0
            0xCE, 0x0F,       // 010A: ADC A,0F   ; soma com carry=1 de antes -> A=00 Z=1 H=1 C=1
            0x3E, 0x00,       // 010C: LD A,00
            0xD6, 0x01,       // 010E: SUB 01     ; A=FF  C=1 H=1 N=1
            0x3E, 0x10,       // 0110: LD A,10
            0xDE, 0x01,       // 0112: SBC A,01   ; subtrai com carry=1 -> A=0E H=1 N=1 C=0
            0x3E, 0xFF,       // 0114: LD A,FF
            0xEE, 0xFF,       // 0116: XOR FF     ; A=00 Z=1
            0x3E, 0x0F,       // 0118: LD A,0F
            0xE6, 0xF0,       // 011A: AND F0     ; A=00 Z=1 H=1(sempre em AND)
            0x3E, 0x80,       // 011C: LD A,80
            0xF6, 0x01,       // 011E: OR 01      ; A=81
            0xFE, 0x81,       // 0120: CP 81      ; Z=1, A continua 81

            0x10, 0x00        // 0122: STOP
        }
    },
    // ESPERADO NO FINAL:
    // A=81 (CP não altera A) | F=C0 (Z=1 N=1 H=0 C=0)
    // pontos a conferir: ADC/SBC puxando o carry da instrução anterior,
    // AND sempre liga H mesmo quando não tem meio-carry "de verdade"

    {
        "inc_dec_hl_mem_wrap",
        0x20,
        {
            0x31, 0xFE, 0xFF, // 0100: LD SP,FFFE
            0x21, 0x00, 0xC0, // 0103: LD HL,C000

            0x36, 0xFF,       // 0106: LD [HL],FF
            0x34,             // 0108: INC [HL]   ; mem=00  Z=1 H=1
            0x35,             // 0109: DEC [HL]   ; mem=FF  H=1 N=1

            0x36, 0x00,       // 010A: LD [HL],00
            0x35,             // 010C: DEC [HL]   ; mem=FF  H=1 N=1 (00-1 dá borrow)
            0x34,             // 010D: INC [HL]   ; mem=00  Z=1 H=1

            0x10, 0x00        // 010E: STOP
        }
    },
    // ESPERADO NO FINAL:
    // mem[C000]=00 | F=A0 (Z=1 N=0 H=1 C=0) — INC não mexe em C, então o C
    // que tava desligado desde o início continua desligado

    {
        "inc_dec_16bit_wrap",
        0x20,
        {
            0x37,             // 0100: SCF          ; liga C só pra provar que INC/DEC 16b não mexe nas flags
            0x01, 0xFF, 0xFF, // 0101: LD BC,FFFF
            0x03,             // 0104: INC BC       ; BC=0000
            0x0B,             // 0105: DEC BC       ; BC=FFFF

            0x11, 0x00, 0x00, // 0106: LD DE,0000
            0x1B,             // 0109: DEC DE       ; DE=FFFF
            0x13,             // 010A: INC DE       ; DE=0000

            0x31, 0xFF, 0xFF, // 010B: LD SP,FFFF
            0x33,             // 010E: INC SP       ; SP=0000
            0x3B,             // 010F: DEC SP       ; SP=FFFF

            0x10, 0x00        // 0110: STOP
        }
    },
    // ESPERADO NO FINAL:
    // BC=FFFF DE=0000 SP=FFFF | F=10 (C continua ligado, só o SCF que setou —
    // se seu INC/DEC de 16 bits mexer em qualquer flag aqui, é bug)

    {
        "add_hl_16bit_bordas",
        0x30,
        {
            0x21, 0xFF, 0x0F, // 0100: LD HL,0FFF
            0x01, 0x01, 0x00, // 0103: LD BC,0001
            0x09,             // 0106: ADD HL,BC   ; HL=1000  H=1 C=0

            0x21, 0xFF, 0xFF, // 0107: LD HL,FFFF
            0x01, 0x01, 0x00, // 010A: LD BC,0001
            0x09,             // 010D: ADD HL,BC   ; HL=0000  H=1 C=1

            0x21, 0x00, 0x80, // 010E: LD HL,8000
            0x29,             // 0111: ADD HL,HL   ; HL=0000  C=1 H=0

            0x31, 0x23, 0x01, // 0112: LD SP,0123
            0x21, 0x00, 0x00, // 0115: LD HL,0000
            0x39,             // 0118: ADD HL,SP   ; HL=0123

            0x10, 0x00        // 0119: STOP
        }
    },
    // ESPERADO NO FINAL:
    // HL=0123 SP=0123 BC=0001 | F=00 (Z e N sempre ficam de fora do ADD HL,
    // só H e C mexem — Z aqui é resquício zerado da última operação)

    {
        "add_sp_e8_ld_hl_sp_e8",
        0x10,
        {
            0x31, 0x00, 0xC0, // 0100: LD SP,C000
            0xE8, 0x10,       // 0103: ADD SP,+10   ; SP=C010
            0xE8, 0xF0,       // 0105: ADD SP,-16   ; SP=C000  (e8=-16 => -0x10)
            0xF8, 0x02,       // 0107: LD HL,SP+2   ; HL=C002
            0xF9,             // 0109: LD SP,HL     ; SP=C002
            0xF8, 0xFE,       // 010A: LD HL,SP-2   ; HL=C000

            0x10, 0x00        // 010C: STOP
        }
    },
    // ESPERADO NO FINAL:
    // HL=C000 SP=C002 | F=30 (Z=0 N=0 H=1 C=1 — vem do ADD SP,-16: soma do
    // byte baixo de SP com 0xF0 estoura o nibble e o byte, então H e C ficam
    // ligados mesmo sendo uma subtração "conceitual"; ADD SP,e8 e LD HL,SP+e8
    // SEMPRE zeram Z e N, e SEMPRE calculam H/C igual ADD de 8 bits, nunca
    // como comparação de 16 bits — erro clássico é comparar SP+e8 inteiro)

    {
        "rotates_carry_chain",
        0x10,
        {
            0x3E, 0x81,       // 0100: LD A,81
            0x07,             // 0102: RLCA   ; A=03  C=1
            0x07,             // 0103: RLCA   ; A=06  C=0
            0x17,             // 0104: RLA    ; A=0C  C=0 (entra o C=0 de antes)

            0x3E, 0x01,       // 0105: LD A,01
            0x1F,             // 0107: RRA    ; A=00  C=1 (bit que saiu vira carry, mas quem ENTRA no bit7 é o C anterior, que era 0)
            0x1F,             // 0108: RRA    ; A=80  C=0 (agora entra o C=1 do passo anterior)
            0x0F,             // 0109: RRCA   ; A=40  C=0

            0x10, 0x00        // 010A: STOP
        }
    },
    // ESPERADO NO FINAL:
    // A=40 | F=00 (Z=0 N=0 H=0 C=0)
    // isso aqui pega documento de emulador errado toda hora: RLA/RRA usam o
    // flag C de ANTES da instrução pra entrar no bit, e não o bit que já
    // saiu na mesma instrução — se seu resultado não bater, é bem provável
    // que seja essa troca de ordem

    {
        "daa_matriz",
        0x30,
        {
            0x3E, 0x09,       // 0100: LD A,09
            0x06, 0x08,       // 0102: LD B,08
            0x80,             // 0104: ADD A,B    ; A=11 (hex), dígito baixo invalido em BCD
            0x27,             // 0105: DAA        ; A=17

            0x3E, 0x90,       // 0106: LD A,90
            0x06, 0x15,       // 0108: LD B,15
            0x80,             // 010A: ADD A,B    ; A=A5
            0x27,             // 010B: DAA        ; A=05  C=1

            0x3E, 0x50,       // 010C: LD A,50
            0x06, 0x50,       // 010E: LD B,50
            0x80,             // 0110: ADD A,B    ; A=A0
            0x27,             // 0111: DAA        ; A=00  Z=1 C=1

            0x3E, 0x00,       // 0112: LD A,00
            0x06, 0x01,       // 0114: LD B,01
            0x90,             // 0116: SUB B      ; A=FF  H=1 C=1 N=1
            0x27,             // 0117: DAA        ; A=99  (ajuste pós-subtração)

            0x3E, 0x42,       // 0118: LD A,42
            0x06, 0x29,       // 011A: LD B,29
            0x90,             // 011C: SUB B      ; A=19  H=1 (2-9 deu borrow no nibble) C=0
            0x27,             // 011D: DAA        ; A=13  (42-29=13 em decimal, confere)

            0x10, 0x00        // 011E: STOP
        }
    },
    // ESPERADO NO FINAL:
    // A=13 B=29 | F=40 (Z=0 N=1 H=0 C=0)
    // essa é a que mais pega bug: repara que a ÚLTIMA DAA (linha 011D) ajusta
    // por causa do H=1 mesmo o resultado hex (0x19) já "parecendo" um BCD
    // válido — se sua DAA só ajustar quando o nibble/byte tá fora da faixa
    // 0-9/00-99 e ignorar as flags H/C, vai dar errado aqui.
    // no modo subtração (N=1), a regra é: se H, subtrai 6; se C, subtrai 0x60;
    // NUNCA soma nesse modo (diferente do modo adição)

    {
        "cpl_scf_ccf_flags",
        0x10,
        {
            0x3E, 0x3C,       // 0100: LD A,3C
            0xFE, 0x3C,       // 0102: CP 3C    ; Z=1 N=1 H=0 C=0
            0x2F,             // 0104: CPL      ; A=C3  N=1 H=1 (Z e C mantidos = 1 e 0)
            0x37,             // 0105: SCF      ; C=1   N=0 H=0 (Z mantido = 1)
            0x3F,             // 0106: CCF      ; C=0   N=0 H=0
            0x3F,             // 0107: CCF      ; C=1   N=0 H=0
            0x2F,             // 0108: CPL      ; A=3C  N=1 H=1 (Z e C mantidos = 1 e 1)

            0x10, 0x00        // 0109: STOP
        }
    },
    // ESPERADO NO FINAL:
    // A=3C | F=F0 (Z=1 N=1 H=1 C=1)
    // ponto de atenção: CPL e SCF/CCF NUNCA tocam no flag Z — só N/H/C.
    // se o seu Z sumir depois de um CPL, procure aí.

    {
        "jp_jr_condicoes",
        0x50,
        {
            0x3E, 0x01,       // 0100: LD A,01
            0xFE, 0x01,       // 0102: CP 01        ; Z=1
            0x28, 0x02,       // 0104: JR Z,+02      -> tomado, pula pro 0108
            0x06, 0xFF,       // 0106: LD B,FF       (pulado)
            0x06, 0x11,       // 0108: LD B,11
            0x20, 0x02,       // 010A: JR NZ,+02     -> Z=1, NÃO tomado
            0x0E, 0x22,       // 010C: LD C,22       (executado, pois não pulou)
            0x0E, 0x33,       // 010E: LD C,33       (executado na sequência)

            0x3E, 0x00,       // 0110: LD A,00
            0xFE, 0x01,       // 0112: CP 01        ; A=00, Z=0
            0xC2, 0x19, 0x01, // 0114: JP NZ,0119    -> tomado
            0x16, 0xFF,       // 0117: LD D,FF       (pulado)
            0x16, 0x44,       // 0119: LD D,44
            0xCA, 0x20, 0x01, // 011B: JP Z,0120     -> Z=0, NÃO tomado
            0x1E, 0x55,       // 011E: LD E,55       (executado)
            0x1E, 0x66,       // 0120: LD E,66       (executado)

            0x37,             // 0122: SCF           ; C=1
            0x30, 0x02,       // 0123: JR NC,+02     -> C=1, NÃO tomado
            0x26, 0x77,       // 0125: LD H,77       (executado)
            0x26, 0x88,       // 0127: LD H,88       (executado)

            0x3F,             // 0129: CCF           ; C=0
            0x38, 0x02,       // 012A: JR C,+02      -> C=0, NÃO tomado
            0x2E, 0x99,       // 012C: LD L,99       (executado)
            0x2E, 0xAA,       // 012E: LD L,AA       (executado)

            0x37,             // 0130: SCF           ; C=1
            0xDA, 0x36, 0x01, // 0131: JP C,0136     -> tomado
            0x06, 0xFF,       // 0134: LD B,FF       (pulado, B continua 11)
            0x3F,             // 0136: CCF           ; C=0
            0xD2, 0x3C, 0x01, // 0137: JP NC,013C    -> tomado
            0x06, 0xEE,       // 013A: LD B,EE       (pulado)

            0x21, 0x42, 0x01, // 013C: LD HL,0142    ; endereço do alvo abaixo
            0xE9,             // 013F: JP [HL]        -> pula pro 0142
            0x3E, 0x00,       // 0140: LD A,00        (pulado)
            0x3E, 0x7F,       // 0142: LD A,7F

            0x10, 0x00        // 0144: STOP
        }
    },
    // ESPERADO NO FINAL:
    // A=7F B=11 C=33 D=44 E=66 H=01 L=42 | F=00 (Z=0 N=0 H=0 C=0)
    // cobre as 8 condições (Z/NZ/C/NC em JR e JP), cada uma tomada e não
    // tomada, mais JP [HL]. Se algum B/C/D/E/H/L final vier diferente,
    // dá pra saber exatamente qual condição/salto errou.

    {
        "call_ret_condicoes",
        0x40,
        {
            0x31, 0xFE, 0xFF, // 0100: LD SP,FFFE
            0x3E, 0x01,       // 0103: LD A,01
            0xFE, 0x01,       // 0105: CP 01         ; Z=1

            0xCC, 0x21, 0x01, // 0107: CALL Z,0121    -> tomado (SUB_Z)
            0xC4, 0x24, 0x01, // 010A: CALL NZ,0124   -> Z=1, NÃO tomado (SUB_NZ)

            0x37,             // 010D: SCF            ; C=1
            0xDC, 0x27, 0x01, // 010E: CALL C,0127    -> tomado (SUB_C)
            0xD4, 0x2A, 0x01, // 0111: CALL NC,012A   -> C=1, NÃO tomado (SUB_NC)

            0xCD, 0x2D, 0x01, // 0114: CALL 012D      (SUB_MAIN, chamada incondicional)

            0x10, 0x00,       // 0117: STOP

            // ---- 0119..0129: sub-rotinas simples ----
            [0x21] = 0x06, [0x22] = 0x01, [0x23] = 0xC9, // 0121: SUB_Z:  LD B,01 / RET
            [0x24] = 0x06, [0x25] = 0xFF, [0x26] = 0xC9, // 0124: SUB_NZ: LD B,FF / RET (nunca deve rodar)
            [0x27] = 0x0E, [0x28] = 0x02, [0x29] = 0xC9, // 0127: SUB_C:  LD C,02 / RET
            [0x2A] = 0x0E, [0x2B] = 0xFF, [0x2C] = 0xC9, // 012A: SUB_NC: LD C,FF / RET (nunca deve rodar)

            // 012D: SUB_MAIN
            [0x2D] = 0x16, [0x2E] = 0x03,               // LD D,03
            [0x2F] = 0xCD, [0x30] = 0x38, [0x31] = 0x01, // CALL 0138 (SUB_INNER, chamada aninhada)
            [0x32] = 0x1E, [0x33] = 0x05,               // LD E,05
            [0x34] = 0xC9,                               // RET

            // 0138: SUB_INNER
            [0x38] = 0x26, [0x39] = 0x04,               // LD H,04
            [0x3A] = 0xC9                                // RET
        }
    },
    // ESPERADO NO FINAL:
    // A=01 B=01 C=02 D=03 E=05 H=04 | F=90 (Z=1 N=0 H=0 C=1, resquício do
    // CP 01 e do SCF — nenhum CALL/RET mexe em flag) | SP=FFFE (voltou pro
    // valor original, prova que toda CALL teve seu RET correspondente e a
    // pilha não vazou nem estourou)
    // se B ou C vierem como FF, é sinal de que uma condição "NÃO tomada"
    // tá sendo tratada como tomada (bug clássico de inverter a condição)

    {
        "push_pop_af_mask",
        0x10,
        {
            0x31, 0xFE, 0xFF, // 0100: LD SP,FFFE
            0x01, 0x12, 0xFF, // 0103: LD BC,FF12     ; B=FF C=12
            0xC5,             // 0106: PUSH BC
            0xF1,             // 0107: POP AF         ; A=FF, F=12&F0=10 (nibble baixo forçado a 0!)
            0xF5,             // 0108: PUSH AF
            0xD1,             // 0109: POP DE         ; D=FF E=10

            0x10, 0x00        // 010A: STOP
        }
    },
    // ESPERADO NO FINAL:
    // A=FF B=FF C=12 D=FF E=10 | F=10 (Z=0 N=0 H=1 C=0)
    // ESSE É O TESTE MAIS IMPORTANTE DO LOTE: o valor empurrado pro F antes
    // do POP tem nibble baixo 0x2, que não é combinação válida de
    // flags — um POP AF correto TEM que zerar esse nibble baixo na hora
    // de carregar em F (F só usa os 4 bits altos: Z N H C, os 4 de baixo
    // são sempre 0). Se seu E final não vier 10, seu POP AF não tá
    // mascarando o F, e isso propaga pra qualquer PUSH AF depois.

    {
        "stack_sp_wraparound",
        0x20,
        {
            0x31, 0x00, 0x00, // 0100: LD SP,0000
            0x01, 0x34, 0x12, // 0103: LD BC,1234
            0xC5,             // 0106: PUSH BC   ; SP dá a volta: 0000 -> FFFE
            0xC1,             // 0107: POP BC    ; SP volta pra 0000, BC=1234 intacto

            0x31, 0xFF, 0xFF, // 0108: LD SP,FFFF
            0x11, 0x78, 0x56, // 010B: LD DE,5678
            0xD5,             // 010E: PUSH DE   ; SP: FFFF -> FFFD
            0xD1,             // 010F: POP DE    ; SP volta pra FFFF, DE=5678 intacto

            0x10, 0x00        // 0110: STOP
        }
    }
    // ESPERADO NO FINAL:
    // BC=1234 DE=5678 | SP=FFFF
    // testa se o PUSH decrementa o SP através do 0x0000 corretamente
    // (0000-1 = FFFF, não deve travar nem zerar errado) e se o POP
    // reconstrói o valor de 16 bits certinho depois do wraparound.

};
// ------------------ end debug   -----------
uint16_t extract_adress(uint8_t upper_byte, uint8_t lower_byte) {
    uint16_t address = ((uint16_t)(upper_byte << 8) | (lower_byte));
    return address;
}

void load_memory(TestProgram program) {
    // TODO(DEBUG): revisar cond_first_run; do jeito atual ele nunca passa para false na primeira carga.
    // TODO(DEBUG): verificar malloc antes de usar e liberar a ROM anterior corretamente.
    if (!cond_first_run) {
        free(memory.game_rom);
        cond_first_run = false;
    }
    memory = (GB_Memory){0};
    memory.game_rom = malloc(sizeof(uint8_t)*16 * 1024);
    memcpy(memory.game_rom+0x0100,program.code,program.size);
    memory.game_rom_lenght = program.size;
}
//______________________________________
uint8_t read_noMBC(uint16_t endereco) {
    return memory.game_rom[endereco];
}
uint8_t read_MBC1(uint16_t endereco) {
    return memory.game_rom[(memory.MBC_curr_bank * 0x4000) + endereco - 0x4000];
}
uint8_t read_MBC2(uint16_t endereco) {
    return memory.game_rom[(memory.MBC_curr_bank * 0x4000) + endereco - 0x4000];
}
uint8_t read_MBC3(uint16_t endereco) {
    return memory.game_rom[(memory.MBC_curr_bank * 0x4000) + endereco - 0x4000];
}

uint8_t read_from_memory_8bit(uint16_t address) {
    // TODO(MEM): validar limites da ROM antes de indexar memory.game_rom.
    // if (address == 0xFF44 && cpu.PC == 0x2828) return 0x91;
    // if (address == 0xFF44) return 0x94;
    if (address >= 0x0000 && 0x3FFF >= address) return memory.game_rom[address];
    if (address >= 0x4000  && 0x7FFF >= address) {
        switch (memory.MBC_type) {
            case 0x00:  return read_noMBC(address);
            case 0x01:  return read_MBC1(address);//TODO: implementar tipo MBC com variaveis
            case 0x02:  return read_MBC1(address);
            case 0x03:  return read_MBC1(address);
        }
    }
    if (address >= 0x8000 && 0x9FFF >= address) return memory.VRAM[address-0x8000];
    if (address >= 0xA000 && 0xBFFF >= address) return memory.cartRAM[address - 0xA000];
    if (address >= 0xC000 && 0xDFFF >= address) return memory.WRAM[address - 0xC000];
    if (address >= 0xE000 && 0xFDFF >= address) return memory.WRAM[address - 0xE000];
    if (address >= 0xFE00 && 0xFE9F >= address) return memory.OAM[address - 0xFE00];
    if (address == 0xFF00) {
        uint8_t joypad = memory.IO[0];
        if (joypad & 0b00100000) {
            uint8_t dummy = joypad & 0b11111111;
            dummy = (on_down_button)?   dummy & 0b11110111 : dummy | 0b00001000;
            dummy = (on_up_button)?     dummy & 0b11111011 : dummy | 0b00000100;
            dummy = (on_left_button)?   dummy & 0b11111101 : dummy | 0b00000010;
            dummy = (on_right_button)?  dummy & 0b11111110 : dummy | 0b00000001;
            return dummy;
        }
        if (joypad & 0b00010000) {
            uint8_t dummy = joypad & 0b11111111;
            dummy = (on_start_button)?  dummy & 0b11110111 : dummy | 0b00001000;
            dummy = (on_select_button)? dummy & 0b11111011 : dummy | 0b00000100;
            dummy = (on_B_button)?      dummy & 0b11111101 : dummy | 0b00000010;
            dummy = (on_A_button)?      dummy & 0b11111110 : dummy | 0b00000001;
            return dummy;
        }
    }
    if (address >= 0xFF00 && 0xFF7F >= address) return memory.IO[address - 0xFF00];
    if (address >= 0xFF80 && 0xFFFE >= address) return memory.HRAM[address - 0xFF80];
    if (address == 0xFFFF) return memory.IE;
    return 0;
}
void write_into_memory_8bit(uint16_t address, uint8_t variable) {
    // TODO(MEM): implementar escrita no Echo RAM (0xE000-0xFDFF).
    // TODO(IO): tratar registradores especiais individualmente (DIV, LY, IF, etc.).
    //if (address >= 0x0000 && 0x3FFF >= address) memory.game_rom[address] = variable;
    //if (address >= 0x4000  && 0x7FFF >= address) {
    //switch (memory.MBC_type) {
    //    case 0x00: memory.game_rom[address] = variable;
    //     case 0x01:  return read_MBC1(address);//TODO: implementar tipo MBC com variaveis
    //     case 0x02:  return read_MBC1(address);
    //     case 0x03:  return read_MBC1(address);
    //}
    //}
    if (address >= 0x8000 && 0x9FFF >= address) memory.VRAM[address-0x8000] = variable;
    else if (address >= 0xA000 && 0xBFFF >= address) memory.cartRAM[address - 0xA000] = variable;
    else if (address >= 0xC000 && 0xDFFF >= address) memory.WRAM[address - 0xC000] = variable;
    else if (address >= 0xFE00 && 0xFE9F >= address) memory.OAM[address - 0xFE00] = variable;
    else if (address >= 0xFF00 && 0xFF7F >= address) {
        if (address == 0xFF02) {
            if (read_from_memory_8bit(0xFF02) & 0x80) {
                memory.IO[address - 0xFF01] =  0xFF;
                memory.IO[address - 0xFF00] = variable & 0x7F;
                set_interrupt(3,true);
            }
            else memory.IO[address - 0xFF00] = variable;
        }
        else if (address == 0xFF46) {
            memory.IO[0x46] = variable;
            cpu.DMA_transfer_pending = true;
            cpu.DMA_transfer_curr_addr = extract_adress(variable, 0x00);
            //system("clear");
            //scan_OAM();
            //getchar();
            cpu.DMA_transfer_limit_addr = cpu.DMA_transfer_curr_addr + 160;
            cpu.DMA_transfer_OAM_addr = 0xFE00;
        }
        else memory.IO[address - 0xFF00] = variable;
    }
    else if (address >= 0xFF80 && 0xFFFE >= address) memory.HRAM[address - 0xFF80] = variable;
    else if (address == 0xFFFF) memory.IE = variable;
}

void write_into_memory_8bit_DMA(uint16_t address, uint8_t variable) {
    // TODO(MEM): implementar escrita no Echo RAM (0xE000-0xFDFF).
    // TODO(IO): tratar registradores especiais individualmente (DIV, LY, IF, etc.).
    //if (address >= 0x0000 && 0x3FFF >= address) memory.game_rom[address] = variable;
    //if (address >= 0x4000  && 0x7FFF >= address) {
    //switch (memory.MBC_type) {
    //    case 0x00: memory.game_rom[address] = variable;
    //     case 0x01:  return read_MBC1(address);//TODO: implementar tipo MBC com variaveis
    //     case 0x02:  return read_MBC1(address);
    //     case 0x03:  return read_MBC1(address);
    //}
    //}
    if (address >= 0x8000 && 0x9FFF >= address) memory.VRAM[address-0x8000] = variable;
    else if (address >= 0xA000 && 0xBFFF >= address) memory.cartRAM[address - 0xA000] = variable;
    else if (address >= 0xC000 && 0xDFFF >= address) memory.WRAM[address - 0xC000] = variable;
    else if (address >= 0xFE00 && 0xFE9F >= address) memory.OAM[address - 0xFE00] = variable;
    else if (address >= 0xFF00 && 0xFF7F >= address) {
        if (address == 0xFF46) {
            memory.IO[address - 0xFF00] = variable;
            cpu.DMA_transfer_pending = true;
            cpu.DMA_transfer_curr_addr = extract_adress(variable, 0x00);
            cpu.DMA_transfer_limit_addr = cpu.DMA_transfer_curr_addr + 160;
            cpu.DMA_transfer_OAM_addr = 0xFE00;
        }
        else memory.IO[address - 0xFF00] = variable;
    }
    else if (address >= 0xFF80 && 0xFFFE >= address) memory.HRAM[address - 0xFF80] = variable;
    else if (address == 0xFFFF) memory.IE = variable;
}
/*
FF0F — IF: Interrupt flag
When an interrupt request signal (some internal wire going from the PPU/APU/… to the CPU) changes from low to high, the corresponding bit in the IF register becomes set. For example, bit 0 becomes set when the PPU enters the VBlank period.
Any set bits in the IF register are only requesting an interrupt. The actual execution of the interrupt handler happens only if both the IME flag and the corresponding bit in the IE register are set; otherwise the interrupt “waits” until both IME and IE allow it to be serviced.
Since the CPU automatically sets and clears the bits in the IF register, it is usually not necessary to write to the IF register. However, the user may still do that in order to manually request (or discard) interrupts. Just like real interrupts, a manually requested interrupt isn’t serviced unless/until IME and IE allow it.

ORDEM:   7 6 5 4 3 2 1 0
         | | | | | | | |
         V V V V V V V V
BITS   0b0 0 0 0 0 0 0 0

0 - VBlank (Read/Write): Controls whether the VBlank interrupt handler is being requested.
1 - LCD (Read/Write): Controls whether the LCD interrupt handler is being requested.
2 - Timer (Read/Write): Controls whether the Timer interrupt handler is being requested.
3 - Serial (Read/Write): Controls whether the Serial interrupt handler is being requested.
4 - Joypad (Read/Write): Controls whether the Joypad interrupt handler is being requested.
5 - blank/nothing
6 - blank/nothing
7 - blank/nothing
 */
void set_interrupt(int bit, bool true_or_false){
    uint8_t interrupt_register = read_from_memory_8bit(0xFF0F);
    switch (bit) {
        case 0: write_into_memory_8bit(0xFF0F,((true_or_false)? interrupt_register | 0b00000001 : interrupt_register & 0b11111110)); break;
        case 1: write_into_memory_8bit(0xFF0F,((true_or_false)? interrupt_register | 0b00000010 : interrupt_register & 0b11111101)); break;
        case 2: write_into_memory_8bit(0xFF0F,((true_or_false)? interrupt_register | 0b00000100 : interrupt_register & 0b11111011)); break;
        case 3: write_into_memory_8bit(0xFF0F,((true_or_false)? interrupt_register | 0b00001000 : interrupt_register & 0b11110111)); break;
        case 4: write_into_memory_8bit(0xFF0F,((true_or_false)? interrupt_register | 0b00010000 : interrupt_register & 0b11101111)); break;
    }
}
/*
LCDC is the main LCD Control register. Its bits toggle what elements are displayed on the screen, and how.

ORDEM:   7 6 5 4 3 2 1 0
         | | | | | | | |
         V V V V V V V V
BITS   0b0 0 0 0 0 0 0 0

7 - LCD & PPU enable    | LCD & PPU enable: 0 = Off; 1 = On
6 - Window tile map     | Window tile map area: 0 = 9800–9BFF; 1 = 9C00–9FFF
5 - Window enable       | Window enable: 0 = Off; 1 = On
4 - BG & Window tiles   | BG & Window tile data area: 0 = 8800–97FF; 1 = 8000–8FFF
3 - BG tile map         | BG tile map area: 0 = 9800–9BFF; 1 = 9C00–9FFF
2 - OBJ size            | OBJ size: 0 = 8×8; 1 = 8×16
1 - OBJ enable          | OBJ enable: 0 = Off; 1 = On
0 - BG & Window enable  | BG & Window enable / priority [Different meaning in CGB Mode]: 0 = Off; 1 = On
 */
bool check_LCDC(int bit_a_ser_checado){
    bool value = false;
    switch (bit_a_ser_checado) {
        case 0: value = ((read_from_memory_8bit(REG_LCDC) & 0b00000001) != 0); break;
        case 1: value = ((read_from_memory_8bit(REG_LCDC) & 0b00000010) != 0); break;
        case 2: value = ((read_from_memory_8bit(REG_LCDC) & 0b00000100) != 0); break;
        case 3: value = ((read_from_memory_8bit(REG_LCDC) & 0b00001000) != 0); break;
        case 4: value = ((read_from_memory_8bit(REG_LCDC) & 0b00010000) != 0); break;
        case 5: value = ((read_from_memory_8bit(REG_LCDC) & 0b00100000) != 0); break;
        case 6: value = ((read_from_memory_8bit(REG_LCDC) & 0b01000000) != 0); break;
        case 7: value = ((read_from_memory_8bit(REG_LCDC) & 0b10000000) != 0); break;
    }
    return value;
}
//FF44 — LY: LCD Y coordinate [read-only]
//LY indicates the current horizontal line, which might be about to be drawn, being drawn, or just been drawn. LY can hold any value from 0 to 153, with values from 144 to 153 indicating the VBlank period.
uint8_t get_curr_LY() {
    return read_from_memory_8bit(0xFF44);
}
void increment_LY(){
    uint8_t ly = get_curr_LY();
    (ly == 153) ? write_into_memory_8bit(0xFF44,0) : write_into_memory_8bit(0xFF44, ly+1);
}
/*
There are various sources which can trigger this interrupt to occur as described in STAT register ($FF41).
The various STAT interrupt sources (modes 0-2 and LYC=LY) have their state (inactive=low and active=high) logically ORed into a shared “STAT interrupt line” if their respective enable bit is turned on.
A STAT interrupt will be triggered by a rising edge (transition from low to high) on the STAT interrupt line.

ORDEM:   7 6 5 4 3 2 1 0
         | | | | | | | |
         V V V V V V V V
BITS   0b0 0 0 0 0 0 0 0

7   - blank/nothing
6   -    LYC int select (Read/Write): If set, selects the LYC == LY cond   for the STAT interrupt.
5   - Mode 2 int select (Read/Write): If set, selects the Mode 2 condition for the STAT interrupt.
4   - Mode 1 int select (Read/Write): If set, selects the Mode 1 condition for the STAT interrupt.
3   - Mode 0 int select (Read/Write): If set, selects the Mode 0 condition for the STAT interrupt.
2   - LYC == LY (Read-only): Set when LY contains the same value as LYC; it is constantly updated.
1,0 - PPU mode (Read-only): Indicates the PPU’s current status. Reports 0 instead when the PPU is disabled.

*/
void set_STAT_mode(int bit_a_ser_mudado, bool on_or_off) {
    switch (bit_a_ser_mudado) {
        case 2: (on_or_off)? (write_into_memory_8bit(REG_STAT,read_from_memory_8bit(REG_STAT) | 0b00000100)) : (write_into_memory_8bit(REG_STAT,read_from_memory_8bit(REG_STAT) & 0b11111011)); break;
        case 3: (on_or_off)? (write_into_memory_8bit(REG_STAT,read_from_memory_8bit(REG_STAT) | 0b00001000)) : (write_into_memory_8bit(REG_STAT,read_from_memory_8bit(REG_STAT) & 0b11110111)); break;
        case 4: (on_or_off)? (write_into_memory_8bit(REG_STAT,read_from_memory_8bit(REG_STAT) | 0b00010000)) : (write_into_memory_8bit(REG_STAT,read_from_memory_8bit(REG_STAT) & 0b11101111)); break;
        case 5: (on_or_off)? (write_into_memory_8bit(REG_STAT,read_from_memory_8bit(REG_STAT) | 0b00100000)) : (write_into_memory_8bit(REG_STAT,read_from_memory_8bit(REG_STAT) & 0b11011111)); break;
        case 6: (on_or_off)? (write_into_memory_8bit(REG_STAT,read_from_memory_8bit(REG_STAT) | 0b01000000)) : (write_into_memory_8bit(REG_STAT,read_from_memory_8bit(REG_STAT) & 0b10111111)); break;
    }
}
/*
There are various sources which can trigger this interrupt to occur as described in STAT register ($FF41).
The various STAT interrupt sources (modes 0-2 and LYC=LY) have their state (inactive=low and active=high) logically ORed into a shared “STAT interrupt line” if their respective enable bit is turned on.
A STAT interrupt will be triggered by a rising edge (transition from low to high) on the STAT interrupt line.

ORDEM:   7 6 5 4 3 2 1 0
         | | | | | | | |
         V V V V V V V V
BITS   0b0 0 0 0 0 0 0 0

7   - blank/nothing
6   -    LYC int select (Read/Write): If set, selects the LYC == LY cond   for the STAT interrupt.
5   - Mode 2 int select (Read/Write): If set, selects the Mode 2 condition for the STAT interrupt.
4   - Mode 1 int select (Read/Write): If set, selects the Mode 1 condition for the STAT interrupt.
3   - Mode 0 int select (Read/Write): If set, selects the Mode 0 condition for the STAT interrupt.
2   - LYC == LY (Read-only): Set when LY contains the same value as LYC; it is constantly updated.
1,0 - PPU mode (Read-only): Indicates the PPU’s current status. Reports 0 instead when the PPU is disabled.

*/
bool get_STAT_mode(int bit) {
    // TODO(PPU): adicionar retorno padrão para bits inválidos e evitar comportamento indefinido.
    switch (bit) {

        case 3: return (read_from_memory_8bit(REG_STAT) & 0b00001000)? true : false;
        case 4: return (read_from_memory_8bit(REG_STAT) & 0b00010000)? true : false;
        case 5: return (read_from_memory_8bit(REG_STAT) & 0b00100000)? true : false;
        case 6: return (read_from_memory_8bit(REG_STAT) & 0b01000000)? true : false;
    }
}
// mode 0 blank horizontal
// mode 1 vblank | blank vertical
// mode 2 OAM scan | prep para o modo 3
// modo 3 modo de draw dos pixels
void set_ppu_mode(uint8_t mode) {
    uint8_t temp_value = read_from_memory_8bit(REG_STAT);
    switch (mode) {
        case 0: write_into_memory_8bit(REG_STAT,(temp_value & 0b11111100)); break;
        case 1: (temp_value |= 0b00000001); (temp_value &= 0b11111101); write_into_memory_8bit(REG_STAT, temp_value); break;
        case 2: (temp_value |= 0b00000010); (temp_value &= 0b11111110); write_into_memory_8bit(REG_STAT, temp_value); break;
        case 3: write_into_memory_8bit(REG_STAT,(temp_value | 0b00000011)); break;
    }
}
bool compare_lyc_ly() {
    if (get_curr_LY() == read_from_memory_8bit(0xFF45)) {
        set_STAT_mode(2, true);
        return true;
    }
    set_STAT_mode(2, false);
    return false;
}
/*
FF45 — LYC: LY compare
The Game Boy constantly compares the value of the LYC and LY registers. When both values are identical, the “LYC=LY” flag in the STAT register is set, and (if enabled) a STAT interrupt is requested.
 */
void check_cycle_counter() {
    // TODO(TIMER): selecionar a frequência usando apenas TAC bits 1-0, sem comparar o byte inteiro.
    // TODO(TIMER): revisar o atraso/comportamento real do overflow de TIMA em uma etapa posterior.
    while (cpu.contador_ciclos_div >= 256) {
        cpu.contador_ciclos_div -= 256;
        memory.IO[0x04] +=1; // div
    }
    if (memory.IO[0x07] & 0b00000100) {
        switch (memory.IO[0x07]) { // TIMA
            case 0b00000101: while (cpu.contador_ciclos_tima >=   16) {
                if (read_from_memory_8bit(0xFF05) == 0xFF) {
                    write_into_memory_8bit(0xFF05, read_from_memory_8bit(0xFF06));
                    cpu.contador_ciclos_tima -= 16;
                    set_interrupt(2,true);
                }
                else {
                    cpu.contador_ciclos_tima -= 16;
                    write_into_memory_8bit(0xFF05, read_from_memory_8bit(0xFF05) + 1);
                }
            }break;
            case 0b00000110: while (cpu.contador_ciclos_tima >=   64) {
                if (read_from_memory_8bit(0xFF05) == 0xFF) {
                    write_into_memory_8bit(0xFF05, read_from_memory_8bit(0xFF06));
                    cpu.contador_ciclos_tima -= 64;
                    set_interrupt(2,true);
                }
                else {
                    cpu.contador_ciclos_tima -= 64;
                    write_into_memory_8bit(0xFF05, read_from_memory_8bit(0xFF05) + 1);
                }
            }break;
            case 0b00000111: while (cpu.contador_ciclos_tima >=  256) {
                if (read_from_memory_8bit(0xFF05) == 0xFF) {
                    write_into_memory_8bit(0xFF05, read_from_memory_8bit(0xFF06));
                    cpu.contador_ciclos_tima -= 256 ;
                    set_interrupt(2,true);
                }
                else {
                    cpu.contador_ciclos_tima -= 256;
                    write_into_memory_8bit(0xFF05, read_from_memory_8bit(0xFF05) + 1);
                }
            }break;
            case 0b00000100: while (cpu.contador_ciclos_tima >= 1024) {
                if (read_from_memory_8bit(0xFF05) == 0xFF) {
                    write_into_memory_8bit(0xFF05, read_from_memory_8bit(0xFF06));
                    cpu.contador_ciclos_tima -= 1024;
                    set_interrupt(2,true);
                }
                else {
                    cpu.contador_ciclos_tima -= 1024;
                    write_into_memory_8bit(0xFF05, read_from_memory_8bit(0xFF05) + 1);
                }
            }break;
        }

    }
}

//__________________________________________
bool is_Z_flag_up() {
    return cpu.F & 0b10000000? true:false;
}
bool is_N_flag_up() {
    return cpu.F & 0b01000000? true:false;
}
bool is_H_flag_up() {
    return cpu.F & 0b00100000? true:false;
}
bool is_C_flag_up() {
    return cpu.F & 0b00010000? true:false;
}

void set_Z_flag_up(bool up_or_down) {
    if (up_or_down) cpu.F = cpu.F | 0b10000000;
    else cpu.F = cpu.F & 0b01111111;
}
void set_N_flag_up(bool up_or_down) {
    if (up_or_down) cpu.F = cpu.F | 0b01000000;
    else cpu.F = cpu.F & 0b10111111;
}
void set_H_flag_up(bool up_or_down) {
    if (up_or_down) cpu.F = cpu.F | 0b00100000;
    else cpu.F = cpu.F & 0b11011111;
}
void set_C_flag_up(bool up_or_down) {
    if (up_or_down) cpu.F = cpu.F | 0b00010000;
    else cpu.F = cpu.F & 0b11101111;
}

//______________________________________

void start_game(char* game_adress) {
    // TODO(ROM): validar fopen antes de usar getc/rewind/fread.
    // TODO(ROM): revisar o +1 no tamanho e conferir o retorno de fread/malloc.
    int game_rom_lenght = 0;
    FILE *fl = fopen(game_adress, "rb");
    int curr_variable = getc(fl);
    while (curr_variable != EOF) {
        game_rom_lenght++;
        curr_variable = getc(fl);
    }
    memory.game_rom_lenght = 1+game_rom_lenght;
    rewind(fl);
    memory.game_rom = malloc(sizeof(uint8_t) * memory.game_rom_lenght);
    fread(memory.game_rom, sizeof(uint8_t), memory.game_rom_lenght-1, fl);
    memory.MBC_curr_bank =1;
    memory.MBC_type = memory.game_rom[0x0147];
    fclose(fl);
}

//______________________________________

int get_opcode_row(uint8_t opcode) {
    uint8_t front_nibble = (opcode & 0xf0);
    return front_nibble;
}
int get_opcode_collum(uint8_t opcode) {
    uint8_t back_nibble = (opcode & 0x0f);
    return back_nibble;
}

//______________________________________
uint8_t update_comparator_AND_8bit_register(uint8_t *target, uint8_t variable) {
    ((*target & variable) == 0) ? set_Z_flag_up(true) : set_Z_flag_up(false);
    set_N_flag_up(false);
    set_H_flag_up(true);
    set_C_flag_up(false);
    *target &= variable;
    return *target;
}
uint8_t update_comparator_OR_8bit_register(uint8_t *target, uint8_t variable) {
    ((*target | variable) == 0) ? set_Z_flag_up(true) : set_Z_flag_up(false);
    set_N_flag_up(false);
    set_H_flag_up(false);
    set_C_flag_up(false);
    *target |= variable;
    return *target;
}
uint8_t update_comparator_XOR_8bit_register(uint8_t *target, uint8_t variable) {
    ((*target ^ variable) == 0) ? set_Z_flag_up(true) : set_Z_flag_up(false);
    set_N_flag_up(false);
    set_H_flag_up(false);
    set_C_flag_up(false);
    *target ^= variable;
    return *target;
}
void update_comparator_CP_8bit_register(uint8_t target, uint8_t variable) {
    ((target - variable) == 0) ? set_Z_flag_up(true) : set_Z_flag_up(false);
    set_N_flag_up(true);
    (((target & 0x0F) < ((variable & 0x0F)))) ? set_H_flag_up(true) : set_H_flag_up(false);
    (((target & 0xFF) < ((variable & 0xFF)))) ? set_C_flag_up(true) : set_C_flag_up(false);
    //printf("\ntarget: 0x%02x | comparator: 0x%02x\n",target, variable);
}
//______________________________________
uint16_t update_decrement_16bit_register(uint8_t *upper_byte, uint8_t *lower_byte, uint8_t decrement) {
    uint16_t target_hex = extract_adress(*upper_byte, *lower_byte);
    uint16_t result = (uint16_t)(((uint16_t)target_hex - decrement) % (0x10000));
    *upper_byte = (uint8_t)((result & 0xFF00) >> 8);
    *lower_byte = (uint8_t)((result & 0xFF));
    return extract_adress(*upper_byte, *lower_byte);
}
void update_increment_SP_e8(int8_t e8) {
    uint16_t old_sp = cpu.SP;
    cpu.SP = (uint16_t)(cpu.SP + e8);
    set_Z_flag_up(false);
    set_N_flag_up(false);
    (((old_sp & 0x0F) + (e8 & 0x0F)) > 0x0F) ? set_H_flag_up(true) : set_H_flag_up(false);
    (((old_sp & 0xFF) + (e8 & 0xFF)) > 0xFF) ? set_C_flag_up(true) : set_C_flag_up(false);
}
uint8_t update_increment_16bit_register(uint8_t *upper_byte, uint8_t *lower_byte, uint16_t increment, bool cond_inc, bool carry_cond) {
    // TODO(CPU): revisar carry_cond; o carry entra em temp_sum/flags, mas não entra no valor de result.
    uint16_t target_hex = extract_adress(*upper_byte, *lower_byte);
    uint8_t carry = 0;
    if (carry_cond) {
        if (is_C_flag_up())
            carry = 1;
    }
    uint32_t temp_sum = target_hex + increment + carry; // & 0xFF0000;
    uint16_t result = (uint16_t)(((uint16_t)target_hex + increment ) % (0x10000));

    if (!cond_inc) set_N_flag_up(false);
    *upper_byte = (uint8_t)((result & 0xFF00) >> 8);
    *lower_byte = (uint8_t)((result & 0xFF));
    if (((target_hex & 0x0FFF) + ((increment+ carry) & 0x0FFF)) > 0x0FFF && !cond_inc)
        set_H_flag_up(true);
    else if (!cond_inc)
        set_H_flag_up(false);
    if (temp_sum & 0xFF0000 && !cond_inc)
        set_C_flag_up(true);
    else if (!cond_inc)
        set_C_flag_up(false);
    return extract_adress(*upper_byte, *lower_byte);
}
uint8_t update_decrement_8bit_register(uint8_t *target, uint8_t decrement, bool cond_dec, bool carry_cond) {
    uint8_t old_value = *target;
    uint8_t carry = 0;
    if (carry_cond) {
        if (is_C_flag_up())
            carry = 1;
    }
    *target = (uint8_t)((uint16_t)(*target) - decrement - carry);
    set_N_flag_up(true);
    (*target == 0)? set_Z_flag_up(true) : set_Z_flag_up(false);
    ((old_value & 0x0F) < ((decrement & 0x0F) + carry)) ? set_H_flag_up(true) : set_H_flag_up(false);
    if (old_value < (decrement + carry) && !cond_dec)
        set_C_flag_up(true);
    else if (!cond_dec)
        set_C_flag_up(false);
    return *target;
}
uint8_t update_increment_8bit_register(uint8_t *target, uint8_t increment, bool cond_inc, bool carry_cond) {
    uint8_t first = (uint8_t) *target & 0x0f;
    uint8_t second= (uint8_t) increment & 0x0f;
    uint8_t carry = 0;
    if (carry_cond) {
        if (is_C_flag_up())
            carry = 1;
    }
    uint16_t result = ((uint16_t)(*target) + increment + carry) & 0x100  ;
    *target = (uint8_t)(((uint16_t)(*target) + increment + carry) % (0x100));
    set_N_flag_up(false);
    (*target == 0) ? set_Z_flag_up(true) : set_Z_flag_up(false);
    ((first+second + carry) & 0x10) ? set_H_flag_up(true) : set_H_flag_up(false);
    if (result && !cond_inc)
        set_C_flag_up(true);
    else if (!cond_inc)
        set_C_flag_up(false);
    return *target;
}
void set_8bit_register(char letter_representing_register, uint8_t variable) {
    switch (letter_representing_register) {
        case 'A': cpu.A = variable; break;
        case 'B': cpu.B = variable; break;
        case 'C': cpu.C = variable; break;
        case 'D': cpu.D = variable; break;
        case 'E': cpu.E = variable; break;
        case 'H': cpu.H = variable; break;
        case 'L': cpu.L = variable; break;
    }
}
//______________________________________
void push_into_stack_16bit(uint8_t upper_byte, uint8_t lower_byte) {
    cpu.SP--;
    write_into_memory_8bit(cpu.SP, upper_byte);
    cpu.SP--;
    write_into_memory_8bit(cpu.SP, lower_byte);
}
void pop_from_stack_to_register_16bit(uint8_t *upper_byte, uint8_t *lower_byte, bool cond_AF) {
    *lower_byte = read_from_memory_8bit(cpu.SP);
    cpu.SP++;
    if (cond_AF) {
        *lower_byte = *lower_byte & 0xF0;
    }
    *upper_byte = read_from_memory_8bit(cpu.SP);
    cpu.SP++;
}
uint16_t pop_from_stack_for_emulator_use_16bit() {
    uint8_t lower_byte, upper_byte;
    lower_byte = read_from_memory_8bit(cpu.SP);
    cpu.SP++;
    upper_byte = read_from_memory_8bit(cpu.SP);
    cpu.SP++;
    return (((uint16_t)(upper_byte)) << 8) | (lower_byte);
}
//______________________________________
void jump_to_address(uint16_t address) {
    cpu.PC = address;
}
void jump_relative(int8_t value) {
    cpu.PC += ((int8_t)value)+2;
}
void call_to_address(uint16_t address) {
    push_into_stack_16bit((uint8_t)((cpu.PC & 0xFF00) >> 8), (uint8_t)(cpu.PC & 0x00FF));
    jump_to_address(address);
}
void return_to_call_address() {
    cpu.PC = pop_from_stack_for_emulator_use_16bit();
}
/*
0xFF04 DIV   → IO[0x04]
0xFF05 TIMA  → IO[0x05]
0xFF06 TMA   → IO[0x06]
0xFF07 TAC   → IO[0x07]
0xFF0F IF    → IO[0x0F]
0xFFFF IE    → memory.IE


bit 0 → VBlank   → vetor 0x0040 → maior prioridade
bit 1 → LCD STAT → vetor 0x0048
bit 2 → Timer    → vetor 0x0050
bit 3 → Serial   → vetor 0x0058
bit 4 → Joypad   → vetor 0x0060 → menor prioridade
*/
bool check_if_is_interrupted(bool only_check_if_has_interruption) {
    if ((read_from_memory_8bit(0xFF0F) & 0b00000001) && (memory.IE & 0b00000001)) { // IME, IF, IE
        if (!only_check_if_has_interruption) {
            set_interrupt(0,false);
            cpu.IME = false;
            call_to_address(0x0040);
        }
        return true;
    }
    if ((read_from_memory_8bit(0xFF0F) & 0b00000010) && (memory.IE & 0b00000010)) { // IME, IF, IE
        if (!only_check_if_has_interruption) {
            set_interrupt(1,false);
            cpu.IME = false;
            call_to_address(0x0048);
        }
        return true;
    }
    if ((read_from_memory_8bit(0xFF0F) & 0b00000100) && (memory.IE & 0b00000100)) { // IME, IF, IE
        if (!only_check_if_has_interruption) {
            set_interrupt(2,false);
            cpu.IME = false;
            call_to_address(0x0050);
        }
        return true;
    }
    if ((read_from_memory_8bit(0xFF0F) & 0b00001000) && (memory.IE & 0b00001000)) { // IME, IF, IE
        if (!only_check_if_has_interruption) {
            set_interrupt(3,false);
            cpu.IME = false;
            call_to_address(0x0058);
        }
        return true;
    }
    if ((read_from_memory_8bit(0xFF0F) & 0b00010000) && (memory.IE & 0b00010000)) { // IME, IF, IE
        if (!only_check_if_has_interruption) {
            set_interrupt(4,false);
            cpu.IME = false;
            call_to_address(0x0060);
        }
        return true;
    }
    return false;
}
//____________________________________________________
void set_16bit_register(char letter_double_register_high_byte, char letter_double_register_lower_byte, uint16_t variable) {
    if (letter_double_register_high_byte == 'B' && letter_double_register_lower_byte == 'C') {
        cpu.B = (uint8_t)((variable & 0xFF00)>>8);
        cpu.C = ((uint8_t)(variable & 0x00FF));
    }
    else if (letter_double_register_high_byte == 'D' && letter_double_register_lower_byte == 'E') {
        cpu.D = (uint8_t)((variable & 0xFF00)>>8);
        cpu.E = ((uint8_t)(variable & 0x00FF));
    }
    else if (letter_double_register_high_byte == 'H' && letter_double_register_lower_byte == 'L') {
        cpu.H = (uint8_t)((variable & 0xFF00)>>8);
        cpu.L = ((uint8_t)(variable & 0x00FF));
    }
}
// reg = ponteiro do registrador
// char left_or_right => L = left shift | R = right shift
// bool cond_carry    => true = include carry "RL" | false = does not include carry "RLC"
void rotate_register(uint8_t* reg, char left_or_right, bool cond_carry_cond) {
    uint8_t old_reg_value = *reg;
    set_H_flag_up(false);
    set_N_flag_up(false);
    if (left_or_right == 'L') {
        *reg = *reg<<1;
        if (cond_carry_cond) {
            if (is_C_flag_up()) {
                set_C_flag_up(false);
                *reg |= 0b1;
            }
            (old_reg_value & 0b10000000)? set_C_flag_up(true) : set_C_flag_up(false);
        }
        else {
            if (old_reg_value & 0b10000000) {
                set_C_flag_up(true);
                *reg |= 0b1;
            }
            else set_C_flag_up(false);
        }
    }
    else if (left_or_right == 'R') {
        *reg = *reg>>1;
        if (cond_carry_cond) {
            if (is_C_flag_up()) {
                set_C_flag_up(false);
                *reg |= 0b10000000;
            }
            (old_reg_value & 0b1)? set_C_flag_up(true) : set_C_flag_up(false);
        }
        else {
            if (old_reg_value & 0b1) {
                set_C_flag_up(true);
                *reg |= 0b10000000;
            }
            else set_C_flag_up(false);
        }
    }
    (*reg == 0)? set_Z_flag_up(true) : set_Z_flag_up(false);
}
void shift_register(uint8_t* reg, char left_or_right, bool is_logical) {
    set_H_flag_up(false);
    set_N_flag_up(false);
    if (left_or_right == 'L'){
        if (*reg & 0b10000000) set_C_flag_up(true);
        else set_C_flag_up(false);
        *reg = *reg << 1;
    }
    else {
        if (*reg & 0b1) set_C_flag_up(true);
        else set_C_flag_up(false);
        if (!is_logical) {
            uint8_t tmp_reg = *reg;
            *reg = *reg >> 1;
            if (tmp_reg & 0b10000000) {
                *reg |=0b10000000;
            }
        }
        else *reg = *reg >> 1;
    }
    (*reg == 0)? set_Z_flag_up(true) : set_Z_flag_up(false);
}
void swap_nibbles(uint8_t* reg) {
    set_C_flag_up(false);
    set_H_flag_up(false);
    set_N_flag_up(false);
    uint8_t lower_nibble = *reg & 0x0F;
    uint8_t upper_nibble = *reg & 0xF0;
    *reg = ((lower_nibble<<4) | (upper_nibble >> 4));
    (*reg == 0)? set_Z_flag_up(true) : set_Z_flag_up(false);
}
void bit_opcode(uint8_t* reg, int bit) {
    set_H_flag_up(true);
    set_N_flag_up(false);
    switch (bit) {
        case 0: (*reg & 0b1)? set_Z_flag_up(false) : set_Z_flag_up(true); break;
        case 1: (*reg & 0b10)? set_Z_flag_up(false) : set_Z_flag_up(true); break;
        case 2: (*reg & 0b100)? set_Z_flag_up(false) : set_Z_flag_up(true); break;
        case 3: (*reg & 0b1000)? set_Z_flag_up(false) : set_Z_flag_up(true); break;
        case 4: (*reg & 0b10000)? set_Z_flag_up(false) : set_Z_flag_up(true); break;
        case 5: (*reg & 0b100000)? set_Z_flag_up(false) : set_Z_flag_up(true); break;
        case 6: (*reg & 0b1000000)? set_Z_flag_up(false) : set_Z_flag_up(true); break;
        case 7: (*reg & 0b10000000)? set_Z_flag_up(false) : set_Z_flag_up(true); break;
    }
}
void flip_bit_opcode(uint8_t* reg, int bit, bool cond_flip_on) {
    if (cond_flip_on) {
        switch (bit) {
            case 0: *reg |= 0b00000001; break;
            case 1: *reg |= 0b00000010; break;
            case 2: *reg |= 0b00000100; break;
            case 3: *reg |= 0b00001000; break;
            case 4: *reg |= 0b00010000; break;
            case 5: *reg |= 0b00100000; break;
            case 6: *reg |= 0b01000000; break;
            case 7: *reg |= 0b10000000; break;
        }
    }
    else {
        switch (bit) {
            case 0: *reg &= 0b11111110; break;
            case 1: *reg &= 0b11111101; break;
            case 2: *reg &= 0b11111011; break;
            case 3: *reg &= 0b11110111; break;
            case 4: *reg &= 0b11101111; break;
            case 5: *reg &= 0b11011111; break;
            case 6: *reg &= 0b10111111; break;
            case 7: *reg &= 0b01111111; break;
        }
    }
}
uint8_t check_operand_row_collum_0_CB_PREFIX(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {

        case 0x00: rotate_register(&cpu.B, 'L', false); cpu.PC+=2; return 8;
        case 0x10: rotate_register(&cpu.B, 'L', true); cpu.PC+=2; return 8;
        case 0x20: shift_register(&cpu.B,'L',false); cpu.PC+=2; return 8;
        case 0x30: swap_nibbles(&cpu.B); cpu.PC+=2; return 8;

        case 0x40: bit_opcode(&cpu.B, 0); cpu.PC+=2; return 8;
        case 0x50: bit_opcode(&cpu.B, 2); cpu.PC+=2; return 8;
        case 0x60: bit_opcode(&cpu.B, 4); cpu.PC+=2; return 8;
        case 0x70: bit_opcode(&cpu.B, 6); cpu.PC+=2; return 8;

        case 0x80: flip_bit_opcode(&cpu.B, 0, false); cpu.PC+=2; return 8;
        case 0x90: flip_bit_opcode(&cpu.B, 2, false); cpu.PC+=2; return 8;
        case 0xA0: flip_bit_opcode(&cpu.B, 4, false); cpu.PC+=2; return 8;
        case 0xB0: flip_bit_opcode(&cpu.B, 6, false); cpu.PC+=2; return 8;

        case 0xC0: flip_bit_opcode(&cpu.B, 0, true); cpu.PC+=2; return 8;
        case 0xD0: flip_bit_opcode(&cpu.B, 2, true); cpu.PC+=2; return 8;
        case 0xE0: flip_bit_opcode(&cpu.B, 4, true); cpu.PC+=2; return 8;
        case 0xF0: flip_bit_opcode(&cpu.B, 6, true); cpu.PC+=2; return 8;
    }
}
uint8_t check_operand_row_collum_1_CB_PREFIX(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: rotate_register(&cpu.C, 'L', false); cpu.PC+=2; return 8;
        case 0x10: rotate_register(&cpu.C, 'L', true); cpu.PC+=2; return 8;
        case 0x20: shift_register(&cpu.C,'L',false); cpu.PC+=2; return 8;
        case 0x30: swap_nibbles(&cpu.C); cpu.PC+=2; return 8;

        case 0x40: bit_opcode(&cpu.C, 0); cpu.PC+=2; return 8;
        case 0x50: bit_opcode(&cpu.C, 2); cpu.PC+=2; return 8;
        case 0x60: bit_opcode(&cpu.C, 4); cpu.PC+=2; return 8;
        case 0x70: bit_opcode(&cpu.C, 6); cpu.PC+=2; return 8;

        case 0x80: flip_bit_opcode(&cpu.C, 0, false); cpu.PC+=2; return 8;
        case 0x90: flip_bit_opcode(&cpu.C, 2, false); cpu.PC+=2; return 8;
        case 0xA0: flip_bit_opcode(&cpu.C, 4, false); cpu.PC+=2; return 8;
        case 0xB0: flip_bit_opcode(&cpu.C, 6, false); cpu.PC+=2; return 8;

        case 0xC0: flip_bit_opcode(&cpu.C, 0, true); cpu.PC+=2; return 8;
        case 0xD0: flip_bit_opcode(&cpu.C, 2, true); cpu.PC+=2; return 8;
        case 0xE0: flip_bit_opcode(&cpu.C, 4, true); cpu.PC+=2; return 8;
        case 0xF0: flip_bit_opcode(&cpu.C, 6, true); cpu.PC+=2; return 8;
    }
}
uint8_t check_operand_row_collum_2_CB_PREFIX(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: rotate_register(&cpu.D, 'L', false); cpu.PC+=2; return 8;
        case 0x10: rotate_register(&cpu.D, 'L', true); cpu.PC+=2; return 8;
        case 0x20: shift_register(&cpu.D,'L',false); cpu.PC+=2; return 8;
        case 0x30: swap_nibbles(&cpu.D); cpu.PC+=2; return 8;

        case 0x40: bit_opcode(&cpu.D, 0); cpu.PC+=2; return 8;
        case 0x50: bit_opcode(&cpu.D, 2); cpu.PC+=2; return 8;
        case 0x60: bit_opcode(&cpu.D, 4); cpu.PC+=2; return 8;
        case 0x70: bit_opcode(&cpu.D, 6); cpu.PC+=2; return 8;

        case 0x80: flip_bit_opcode(&cpu.D, 0, false); cpu.PC+=2; return 8;
        case 0x90: flip_bit_opcode(&cpu.D, 2, false); cpu.PC+=2; return 8;
        case 0xA0: flip_bit_opcode(&cpu.D, 4, false); cpu.PC+=2; return 8;
        case 0xB0: flip_bit_opcode(&cpu.D, 6, false); cpu.PC+=2; return 8;

        case 0xC0: flip_bit_opcode(&cpu.D, 0, true); cpu.PC+=2; return 8;
        case 0xD0: flip_bit_opcode(&cpu.D, 2, true); cpu.PC+=2; return 8;
        case 0xE0: flip_bit_opcode(&cpu.D, 4, true); cpu.PC+=2; return 8;
        case 0xF0: flip_bit_opcode(&cpu.D, 6, true); cpu.PC+=2; return 8;
    }
}
uint8_t check_operand_row_collum_3_CB_PREFIX(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: rotate_register(&cpu.E, 'L', false); cpu.PC+=2; return 8;
        case 0x10: rotate_register(&cpu.E, 'L', true); cpu.PC+=2; return 8;
        case 0x20: shift_register(&cpu.E,'L',false); cpu.PC+=2; return 8;
        case 0x30: swap_nibbles(&cpu.E); cpu.PC+=2; return 8;

        case 0x40: bit_opcode(&cpu.E, 0); cpu.PC+=2; return 8;
        case 0x50: bit_opcode(&cpu.E, 2); cpu.PC+=2; return 8;
        case 0x60: bit_opcode(&cpu.E, 4); cpu.PC+=2; return 8;
        case 0x70: bit_opcode(&cpu.E, 6); cpu.PC+=2; return 8;

        case 0x80: flip_bit_opcode(&cpu.E, 0, false); cpu.PC+=2; return 8;
        case 0x90: flip_bit_opcode(&cpu.E, 2, false); cpu.PC+=2; return 8;
        case 0xA0: flip_bit_opcode(&cpu.E, 4, false); cpu.PC+=2; return 8;
        case 0xB0: flip_bit_opcode(&cpu.E, 6, false); cpu.PC+=2; return 8;

        case 0xC0: flip_bit_opcode(&cpu.E, 0, true); cpu.PC+=2; return 8;
        case 0xD0: flip_bit_opcode(&cpu.E, 2, true); cpu.PC+=2; return 8;
        case 0xE0: flip_bit_opcode(&cpu.E, 4, true); cpu.PC+=2; return 8;
        case 0xF0: flip_bit_opcode(&cpu.E, 6, true); cpu.PC+=2; return 8;
    }
}
uint8_t check_operand_row_collum_4_CB_PREFIX(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: rotate_register(&cpu.H, 'L', false); cpu.PC+=2; return 8;
        case 0x10: rotate_register(&cpu.H, 'L', true); cpu.PC+=2; return 8;
        case 0x20: shift_register(&cpu.H,'L',false); cpu.PC+=2; return 8;
        case 0x30: swap_nibbles(&cpu.H); cpu.PC+=2; return 8;

        case 0x40: bit_opcode(&cpu.H, 0); cpu.PC+=2; return 8;
        case 0x50: bit_opcode(&cpu.H, 2); cpu.PC+=2; return 8;
        case 0x60: bit_opcode(&cpu.H, 4); cpu.PC+=2; return 8;
        case 0x70: bit_opcode(&cpu.H, 6); cpu.PC+=2; return 8;

        case 0x80: flip_bit_opcode(&cpu.H, 0, false); cpu.PC+=2; return 8;
        case 0x90: flip_bit_opcode(&cpu.H, 2, false); cpu.PC+=2; return 8;
        case 0xA0: flip_bit_opcode(&cpu.H, 4, false); cpu.PC+=2; return 8;
        case 0xB0: flip_bit_opcode(&cpu.H, 6, false); cpu.PC+=2; return 8;

        case 0xC0: flip_bit_opcode(&cpu.H, 0, true); cpu.PC+=2; return 8;
        case 0xD0: flip_bit_opcode(&cpu.H, 2, true); cpu.PC+=2; return 8;
        case 0xE0: flip_bit_opcode(&cpu.H, 4, true); cpu.PC+=2; return 8;
        case 0xF0: flip_bit_opcode(&cpu.H, 6, true); cpu.PC+=2; return 8;
    }
}
uint8_t check_operand_row_collum_5_CB_PREFIX(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: rotate_register(&cpu.L, 'L', false); cpu.PC+=2; return 8;
        case 0x10: rotate_register(&cpu.L, 'L', true); cpu.PC+=2; return 8;
        case 0x20: shift_register(&cpu.L,'L',false); cpu.PC+=2; return 8;
        case 0x30: swap_nibbles(&cpu.L); cpu.PC+=2; return 8;

        case 0x40: bit_opcode(&cpu.L, 0); cpu.PC+=2; return 8;
        case 0x50: bit_opcode(&cpu.L, 2); cpu.PC+=2; return 8;
        case 0x60: bit_opcode(&cpu.L, 4); cpu.PC+=2; return 8;
        case 0x70: bit_opcode(&cpu.L, 6); cpu.PC+=2; return 8;

        case 0x80: flip_bit_opcode(&cpu.L, 0, false); cpu.PC+=2; return 8;
        case 0x90: flip_bit_opcode(&cpu.L, 2, false); cpu.PC+=2; return 8;
        case 0xA0: flip_bit_opcode(&cpu.L, 4, false); cpu.PC+=2; return 8;
        case 0xB0: flip_bit_opcode(&cpu.L, 6, false); cpu.PC+=2; return 8;

        case 0xC0: flip_bit_opcode(&cpu.L, 0, true); cpu.PC+=2; return 8;
        case 0xD0: flip_bit_opcode(&cpu.L, 2, true); cpu.PC+=2; return 8;
        case 0xE0: flip_bit_opcode(&cpu.L, 4, true); cpu.PC+=2; return 8;
        case 0xF0: flip_bit_opcode(&cpu.L, 6, true); cpu.PC+=2; return 8;
    }
}
uint8_t check_operand_row_collum_6_CB_PREFIX(uint8_t opcode) {
    uint8_t temp = read_from_memory_8bit(extract_adress(cpu.H, cpu.L));
    switch (get_opcode_row(opcode)) {
        case 0x00: rotate_register(&temp, 'L', false); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0x10: rotate_register(&temp, 'L', true); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0x20: shift_register(&temp,'L',false); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0x30: swap_nibbles(&temp); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;

        case 0x40: bit_opcode(&temp, 0); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 12;
        case 0x50: bit_opcode(&temp, 2); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 12;
        case 0x60: bit_opcode(&temp, 4); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 12;
        case 0x70: bit_opcode(&temp, 6); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 12;

        case 0x80: flip_bit_opcode(&temp, 0, false); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0x90: flip_bit_opcode(&temp, 2, false); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0xA0: flip_bit_opcode(&temp, 4, false); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0xB0: flip_bit_opcode(&temp, 6, false); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;

        case 0xC0: flip_bit_opcode(&temp, 0, true); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0xD0: flip_bit_opcode(&temp, 2, true); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0xE0: flip_bit_opcode(&temp, 4, true); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0xF0: flip_bit_opcode(&temp, 6, true); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
    }
}
uint8_t check_operand_row_collum_7_CB_PREFIX(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: rotate_register(&cpu.A, 'L', false); cpu.PC+=2; return 8;
        case 0x10: rotate_register(&cpu.A, 'L', true); cpu.PC+=2; return 8;
        case 0x20: shift_register(&cpu.A,'L',false); cpu.PC+=2; return 8;
        case 0x30: swap_nibbles(&cpu.A); cpu.PC+=2; return 8;

        case 0x40: bit_opcode(&cpu.A, 0); cpu.PC+=2; return 8;
        case 0x50: bit_opcode(&cpu.A, 2); cpu.PC+=2; return 8;
        case 0x60: bit_opcode(&cpu.A, 4); cpu.PC+=2; return 8;
        case 0x70: bit_opcode(&cpu.A, 6); cpu.PC+=2; return 8;

        case 0x80: flip_bit_opcode(&cpu.A, 0, false); cpu.PC+=2; return 8;
        case 0x90: flip_bit_opcode(&cpu.A, 2, false); cpu.PC+=2; return 8;
        case 0xA0: flip_bit_opcode(&cpu.A, 4, false); cpu.PC+=2; return 8;
        case 0xB0: flip_bit_opcode(&cpu.A, 6, false); cpu.PC+=2; return 8;

        case 0xC0: flip_bit_opcode(&cpu.A, 0, true); cpu.PC+=2; return 8;
        case 0xD0: flip_bit_opcode(&cpu.A, 2, true); cpu.PC+=2; return 8;
        case 0xE0: flip_bit_opcode(&cpu.A, 4, true); cpu.PC+=2; return 8;
        case 0xF0: flip_bit_opcode(&cpu.A, 6, true); cpu.PC+=2; return 8;
    }
}
uint8_t check_operand_row_collum_8_CB_PREFIX(uint8_t opcode) {
    switch (get_opcode_row(opcode))  {
        case 0x00: rotate_register(&cpu.B, 'R', false); cpu.PC+=2; return 8;
        case 0x10: rotate_register(&cpu.B, 'R', true); cpu.PC+=2; return 8;
        case 0x20: shift_register(&cpu.B,'R',false); cpu.PC+=2; return 8;
        case 0x30: shift_register(&cpu.B,'R',true); cpu.PC+=2; return 8;

        case 0x40: bit_opcode(&cpu.B, 1); cpu.PC+=2; return 8;
        case 0x50: bit_opcode(&cpu.B, 3); cpu.PC+=2; return 8;
        case 0x60: bit_opcode(&cpu.B, 5); cpu.PC+=2; return 8;
        case 0x70: bit_opcode(&cpu.B, 7); cpu.PC+=2; return 8;

        case 0x80: flip_bit_opcode(&cpu.B, 1, false); cpu.PC+=2; return 8;
        case 0x90: flip_bit_opcode(&cpu.B, 3, false); cpu.PC+=2; return 8;
        case 0xA0: flip_bit_opcode(&cpu.B, 5, false); cpu.PC+=2; return 8;
        case 0xB0: flip_bit_opcode(&cpu.B, 7, false); cpu.PC+=2; return 8;

        case 0xC0: flip_bit_opcode(&cpu.B, 1, true); cpu.PC+=2; return 8;
        case 0xD0: flip_bit_opcode(&cpu.B, 3, true); cpu.PC+=2; return 8;
        case 0xE0: flip_bit_opcode(&cpu.B, 5, true); cpu.PC+=2; return 8;
        case 0xF0: flip_bit_opcode(&cpu.B, 7, true); cpu.PC+=2; return 8;
    }
}
uint8_t check_operand_row_collum_9_CB_PREFIX(uint8_t opcode) {
    switch (get_opcode_row(opcode))  {
        case 0x00: rotate_register(&cpu.C, 'R', false); cpu.PC+=2; return 8;
        case 0x10: rotate_register(&cpu.C, 'R', true); cpu.PC+=2; return 8;
        case 0x20: shift_register(&cpu.C,'R',false); cpu.PC+=2; return 8;
        case 0x30: shift_register(&cpu.C,'R',true); cpu.PC+=2; return 8;

        case 0x40: bit_opcode(&cpu.C, 1); cpu.PC+=2; return 8;
        case 0x50: bit_opcode(&cpu.C, 3); cpu.PC+=2; return 8;
        case 0x60: bit_opcode(&cpu.C, 5); cpu.PC+=2; return 8;
        case 0x70: bit_opcode(&cpu.C, 7); cpu.PC+=2; return 8;

        case 0x80: flip_bit_opcode(&cpu.C, 1, false); cpu.PC+=2; return 8;
        case 0x90: flip_bit_opcode(&cpu.C, 3, false); cpu.PC+=2; return 8;
        case 0xA0: flip_bit_opcode(&cpu.C, 5, false); cpu.PC+=2; return 8;
        case 0xB0: flip_bit_opcode(&cpu.C, 7, false); cpu.PC+=2; return 8;

        case 0xC0: flip_bit_opcode(&cpu.C, 1, true); cpu.PC+=2; return 8;
        case 0xD0: flip_bit_opcode(&cpu.C, 3, true); cpu.PC+=2; return 8;
        case 0xE0: flip_bit_opcode(&cpu.C, 5, true); cpu.PC+=2; return 8;
        case 0xF0: flip_bit_opcode(&cpu.C, 7, true); cpu.PC+=2; return 8;
    }
}
uint8_t check_operand_row_collum_A_CB_PREFIX(uint8_t opcode) {
    switch (get_opcode_row(opcode))  {
        case 0x00: rotate_register(&cpu.D, 'R', false); cpu.PC+=2; return 8;
        case 0x10: rotate_register(&cpu.D, 'R', true); cpu.PC+=2; return 8;
        case 0x20: shift_register(&cpu.D,'R',false); cpu.PC+=2; return 8;
        case 0x30: shift_register(&cpu.D,'R',true); cpu.PC+=2; return 8;

        case 0x40: bit_opcode(&cpu.D, 1); cpu.PC+=2; return 8;
        case 0x50: bit_opcode(&cpu.D, 3); cpu.PC+=2; return 8;
        case 0x60: bit_opcode(&cpu.D, 5); cpu.PC+=2; return 8;
        case 0x70: bit_opcode(&cpu.D, 7); cpu.PC+=2; return 8;

        case 0x80: flip_bit_opcode(&cpu.D, 1, false); cpu.PC+=2; return 8;
        case 0x90: flip_bit_opcode(&cpu.D, 3, false); cpu.PC+=2; return 8;
        case 0xA0: flip_bit_opcode(&cpu.D, 5, false); cpu.PC+=2; return 8;
        case 0xB0: flip_bit_opcode(&cpu.D, 7, false); cpu.PC+=2; return 8;

        case 0xC0: flip_bit_opcode(&cpu.D, 1, true); cpu.PC+=2; return 8;
        case 0xD0: flip_bit_opcode(&cpu.D, 3, true); cpu.PC+=2; return 8;
        case 0xE0: flip_bit_opcode(&cpu.D, 5, true); cpu.PC+=2; return 8;
        case 0xF0: flip_bit_opcode(&cpu.D, 7, true); cpu.PC+=2; return 8;
    }
}
uint8_t check_operand_row_collum_B_CB_PREFIX(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: rotate_register(&cpu.E, 'R', false); cpu.PC+=2; return 8;
        case 0x10: rotate_register(&cpu.E, 'R', true); cpu.PC+=2; return 8;
        case 0x20: shift_register(&cpu.E,'R',false); cpu.PC+=2; return 8;
        case 0x30: shift_register(&cpu.E,'R',true); cpu.PC+=2; return 8;

        case 0x40: bit_opcode(&cpu.E, 1); cpu.PC+=2; return 8;
        case 0x50: bit_opcode(&cpu.E, 3); cpu.PC+=2; return 8;
        case 0x60: bit_opcode(&cpu.E, 5); cpu.PC+=2; return 8;
        case 0x70: bit_opcode(&cpu.E, 7); cpu.PC+=2; return 8;

        case 0x80: flip_bit_opcode(&cpu.E, 1, false); cpu.PC+=2; return 8;
        case 0x90: flip_bit_opcode(&cpu.E, 3, false); cpu.PC+=2; return 8;
        case 0xA0: flip_bit_opcode(&cpu.E, 5, false); cpu.PC+=2; return 8;
        case 0xB0: flip_bit_opcode(&cpu.E, 7, false); cpu.PC+=2; return 8;

        case 0xC0: flip_bit_opcode(&cpu.E, 1, true); cpu.PC+=2; return 8;
        case 0xD0: flip_bit_opcode(&cpu.E, 3, true); cpu.PC+=2; return 8;
        case 0xE0: flip_bit_opcode(&cpu.E, 5, true); cpu.PC+=2; return 8;
        case 0xF0: flip_bit_opcode(&cpu.E, 7, true); cpu.PC+=2; return 8;
    }
}
uint8_t check_operand_row_collum_C_CB_PREFIX(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: rotate_register(&cpu.H, 'R', false); cpu.PC+=2; return 8;
        case 0x10: rotate_register(&cpu.H, 'R', true); cpu.PC+=2; return 8;
        case 0x20: shift_register(&cpu.H,'R',false); cpu.PC+=2; return 8;
        case 0x30: shift_register(&cpu.H,'R',true); cpu.PC+=2; return 8;

        case 0x40: bit_opcode(&cpu.H, 1); cpu.PC+=2; return 8;
        case 0x50: bit_opcode(&cpu.H, 3); cpu.PC+=2; return 8;
        case 0x60: bit_opcode(&cpu.H, 5); cpu.PC+=2; return 8;
        case 0x70: bit_opcode(&cpu.H, 7); cpu.PC+=2; return 8;

        case 0x80: flip_bit_opcode(&cpu.H, 1, false); cpu.PC+=2; return 8;
        case 0x90: flip_bit_opcode(&cpu.H, 3, false); cpu.PC+=2; return 8;
        case 0xA0: flip_bit_opcode(&cpu.H, 5, false); cpu.PC+=2; return 8;
        case 0xB0: flip_bit_opcode(&cpu.H, 7, false); cpu.PC+=2; return 8;

        case 0xC0: flip_bit_opcode(&cpu.H, 1, true); cpu.PC+=2; return 8;
        case 0xD0: flip_bit_opcode(&cpu.H, 3, true); cpu.PC+=2; return 8;
        case 0xE0: flip_bit_opcode(&cpu.H, 5, true); cpu.PC+=2; return 8;
        case 0xF0: flip_bit_opcode(&cpu.H, 7, true); cpu.PC+=2; return 8;
    }
}
uint8_t check_operand_row_collum_D_CB_PREFIX(uint8_t opcode) {
    switch (get_opcode_row(opcode))  {
        case 0x00: rotate_register(&cpu.L, 'R', false); cpu.PC+=2; return 8;
        case 0x10: rotate_register(&cpu.L, 'R', true); cpu.PC+=2; return 8;
        case 0x20: shift_register(&cpu.L,'R',false); cpu.PC+=2; return 8;
        case 0x30: shift_register(&cpu.L,'R',true); cpu.PC+=2; return 8;

        case 0x40: bit_opcode(&cpu.L, 1); cpu.PC+=2; return 8;
        case 0x50: bit_opcode(&cpu.L, 3); cpu.PC+=2; return 8;
        case 0x60: bit_opcode(&cpu.L, 5); cpu.PC+=2; return 8;
        case 0x70: bit_opcode(&cpu.L, 7); cpu.PC+=2; return 8;

        case 0x80: flip_bit_opcode(&cpu.L, 1, false); cpu.PC+=2; return 8;
        case 0x90: flip_bit_opcode(&cpu.L, 3, false); cpu.PC+=2; return 8;
        case 0xA0: flip_bit_opcode(&cpu.L, 5, false); cpu.PC+=2; return 8;
        case 0xB0: flip_bit_opcode(&cpu.L, 7, false); cpu.PC+=2; return 8;

        case 0xC0: flip_bit_opcode(&cpu.L, 1, true); cpu.PC+=2; return 8;
        case 0xD0: flip_bit_opcode(&cpu.L, 3, true); cpu.PC+=2; return 8;
        case 0xE0: flip_bit_opcode(&cpu.L, 5, true); cpu.PC+=2; return 8;
        case 0xF0: flip_bit_opcode(&cpu.L, 7, true); cpu.PC+=2; return 8;
    }
}
uint8_t check_operand_row_collum_E_CB_PREFIX(uint8_t opcode) {
    uint8_t temp = read_from_memory_8bit(extract_adress(cpu.H, cpu.L));
    write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp);
    switch (get_opcode_row(opcode)) {
        case 0x00: rotate_register(&temp, 'R', false); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0x10: rotate_register(&temp, 'R', true); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0x20: shift_register(&temp,'R',false); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0x30: shift_register(&temp,'R',true); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;

        case 0x40: bit_opcode(&temp, 1); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 12;
        case 0x50: bit_opcode(&temp, 3); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 12;
        case 0x60: bit_opcode(&temp, 5); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 12;
        case 0x70: bit_opcode(&temp, 7); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 12;

        case 0x80: flip_bit_opcode(&temp, 1, false); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0x90: flip_bit_opcode(&temp, 3, false); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0xA0: flip_bit_opcode(&temp, 5, false); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0xB0: flip_bit_opcode(&temp, 7, false); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;

        case 0xC0: flip_bit_opcode(&temp, 1, true); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0xD0: flip_bit_opcode(&temp, 3, true); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0xE0: flip_bit_opcode(&temp, 5, true); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
        case 0xF0: flip_bit_opcode(&temp, 7, true); write_into_memory_8bit(extract_adress(cpu.H, cpu.L),temp); cpu.PC+=2; return 16;
    }
}
uint8_t check_operand_row_collum_F_CB_PREFIX(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: rotate_register(&cpu.A, 'R', false); cpu.PC+=2; return 8;
        case 0x10: rotate_register(&cpu.A, 'R', true); cpu.PC+=2; return 8;
        case 0x20: shift_register(&cpu.A,'R',false); cpu.PC+=2; return 8;
        case 0x30: shift_register(&cpu.A,'R',true); cpu.PC+=2; return 8;

        case 0x40: bit_opcode(&cpu.A, 1); cpu.PC+=2; return 8;
        case 0x50: bit_opcode(&cpu.A, 3); cpu.PC+=2; return 8;
        case 0x60: bit_opcode(&cpu.A, 5); cpu.PC+=2; return 8;
        case 0x70: bit_opcode(&cpu.A, 7); cpu.PC+=2; return 8;

        case 0x80: flip_bit_opcode(&cpu.A, 1, false); cpu.PC+=2; return 8;
        case 0x90: flip_bit_opcode(&cpu.A, 3, false); cpu.PC+=2; return 8;
        case 0xA0: flip_bit_opcode(&cpu.A, 5, false); cpu.PC+=2; return 8;
        case 0xB0: flip_bit_opcode(&cpu.A, 7, false); cpu.PC+=2; return 8;

        case 0xC0: flip_bit_opcode(&cpu.A, 1, true); cpu.PC+=2; return 8;
        case 0xD0: flip_bit_opcode(&cpu.A, 3, true); cpu.PC+=2; return 8;
        case 0xE0: flip_bit_opcode(&cpu.A, 5, true); cpu.PC+=2; return 8;
        case 0xF0: flip_bit_opcode(&cpu.A, 7, true); cpu.PC+=2; return 8;
    }
}
uint8_t check_operand_collumn_CB_PREFIX(uint8_t opcode) {
    switch (get_opcode_collum(opcode)) {
        case 0x00: return check_operand_row_collum_0_CB_PREFIX(opcode);
        case 0x01: return check_operand_row_collum_1_CB_PREFIX(opcode);
        case 0x02: return check_operand_row_collum_2_CB_PREFIX(opcode);
        case 0x03: return check_operand_row_collum_3_CB_PREFIX(opcode);
        case 0x04: return check_operand_row_collum_4_CB_PREFIX(opcode);
        case 0x05: return check_operand_row_collum_5_CB_PREFIX(opcode);
        case 0x06: return check_operand_row_collum_6_CB_PREFIX(opcode);
        case 0x07: return check_operand_row_collum_7_CB_PREFIX(opcode);
        case 0x08: return check_operand_row_collum_8_CB_PREFIX(opcode);
        case 0x09: return check_operand_row_collum_9_CB_PREFIX(opcode);
        case 0x0A: return check_operand_row_collum_A_CB_PREFIX(opcode);
        case 0x0B: return check_operand_row_collum_B_CB_PREFIX(opcode);
        case 0x0C: return check_operand_row_collum_C_CB_PREFIX(opcode);
        case 0x0D: return check_operand_row_collum_D_CB_PREFIX(opcode);
        case 0x0E: return check_operand_row_collum_E_CB_PREFIX(opcode);
        case 0x0F: return check_operand_row_collum_F_CB_PREFIX(opcode);
    }
}
//____________________________________________________
uint8_t check_operand_row_collum_0(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: cpu.PC++; return 4;
        case 0x10: cpu.is_halted = true; cpu.PC+=2; return 4;
        case 0x20:
            if (!is_Z_flag_up()) {
                jump_relative((int8_t)read_from_memory_8bit(cpu.PC+1));
                return 12;
            } cpu.PC += 2; return 8;
        case 0x30:
            if (!is_C_flag_up()) {
                jump_relative((int8_t)read_from_memory_8bit(cpu.PC+1));
                return 12;
            } cpu.PC += 2; return 8;

        case 0x40: cpu.PC++; return 4;
        case 0x50: set_8bit_register('D',cpu.B); cpu.PC++; return 4;
        case 0x60: set_8bit_register('H', cpu.B); cpu.PC++; return 4;
        case 0x70: write_into_memory_8bit(extract_adress(cpu.H,cpu.L), cpu.B); cpu.PC++; return 8;

        case 0x80: update_increment_8bit_register(&cpu.A, cpu.B, false, false); cpu.PC++; return 4;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.B, false, false); cpu.PC++; return 4;
        case 0xA0: update_comparator_AND_8bit_register(&cpu.A, cpu.B); cpu.PC++; return 4;
        case 0xB0: update_comparator_OR_8bit_register(&cpu.A, cpu.B); cpu.PC++; return 4;

        case 0xC0:
            if (!is_Z_flag_up()) {
                return_to_call_address();
                return 20;
            } cpu.PC++; return 8;
        case 0xD0:
            if (!is_C_flag_up()) {
                return_to_call_address();
                return 20;
            } cpu.PC++; return 8;
        case 0xE0: write_into_memory_8bit(extract_adress(0xFF, read_from_memory_8bit(cpu.PC +1)), cpu.A); cpu.PC+=2; return 12;
        case 0xF0:
            set_8bit_register('A',read_from_memory_8bit(extract_adress(0xFF, read_from_memory_8bit(cpu.PC +1)))); cpu.PC+=2; return 12;
        default:
    }
}
uint8_t check_operand_row_collum_1(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: set_16bit_register('B','C',extract_adress(read_from_memory_8bit(cpu.PC +2),read_from_memory_8bit(cpu.PC +1))); cpu.PC+=3; return 12;
        case 0x10: set_16bit_register('D','E',extract_adress(read_from_memory_8bit(cpu.PC +2),read_from_memory_8bit(cpu.PC +1))); cpu.PC+=3; return 12;  // load HL, n16
        case 0x20: set_16bit_register('H','L',extract_adress(read_from_memory_8bit(cpu.PC +2),read_from_memory_8bit(cpu.PC +1))); cpu.PC+=3; return 12;  // load
        case 0x30: cpu.SP = extract_adress(read_from_memory_8bit(cpu.PC +2),read_from_memory_8bit(cpu.PC +1)); cpu.PC+=3; return 12;

        case 0x40: set_8bit_register('B', cpu.C); cpu.PC++; return 4;
        case 0x50: set_8bit_register('D', cpu.C); cpu.PC++; return 4;
        case 0x60: set_8bit_register('H', cpu.C); cpu.PC++; return 4;
        case 0x70: write_into_memory_8bit(extract_adress(cpu.H,cpu.L), cpu.C); cpu.PC++; return 8;

        case 0x80: update_increment_8bit_register(&cpu.A, cpu.C, false, false); cpu.PC++; return 4;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.C, false, false); cpu.PC++; return 4;
        case 0xA0: update_comparator_AND_8bit_register(&cpu.A, cpu.C); cpu.PC++; return 4;
        case 0xB0: update_comparator_OR_8bit_register(&cpu.A, cpu.C); cpu.PC++; return 4;

        case 0xC0: pop_from_stack_to_register_16bit(&cpu.B,&cpu.C,false); cpu.PC++;return 12;
        case 0xD0: pop_from_stack_to_register_16bit(&cpu.D,&cpu.E,false); cpu.PC++;return 12;
        case 0xE0: pop_from_stack_to_register_16bit(&cpu.H,&cpu.L,false); cpu.PC++;return 12;
        case 0xF0: pop_from_stack_to_register_16bit(&cpu.A,&cpu.F,true); cpu.PC++;return 12;
        default:
    }
}
uint8_t check_operand_row_collum_2(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: write_into_memory_8bit(extract_adress(cpu.B,cpu.C), cpu.A); cpu.PC++; return 8;
        case 0x10: write_into_memory_8bit(extract_adress(cpu.D,cpu.E), cpu.A); cpu.PC++; return 8;
        case 0x20:
            write_into_memory_8bit(extract_adress(cpu.H,cpu.L), cpu.A);
            update_increment_16bit_register(&cpu.H, &cpu.L, 1, true, false); cpu.PC++; return 8;
        case 0x30:
            write_into_memory_8bit(extract_adress(cpu.H,cpu.L), cpu.A);
            update_decrement_16bit_register(&cpu.H, &cpu.L, 1); cpu.PC++; return 8;

        case 0x40: set_8bit_register('B', cpu.D); cpu.PC++; return 4;
        case 0x50: cpu.PC++; return 4;
        case 0x60: set_8bit_register('H', cpu.D); cpu.PC++; return 4;
        case 0x70: write_into_memory_8bit(extract_adress(cpu.H, cpu.L), cpu.D); cpu.PC++; return 8;

        case 0x80: update_increment_8bit_register(&cpu.A, cpu.D, false, false); cpu.PC++; return 4;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.D, false, false); cpu.PC++; return 4;
        case 0xA0: update_comparator_AND_8bit_register(&cpu.A, cpu.D); cpu.PC++; return 4;
        case 0xB0: update_comparator_OR_8bit_register(&cpu.A, cpu.D); cpu.PC++; return 4;

        case 0xC0:
            if (!is_Z_flag_up()) {
                jump_to_address(extract_adress(read_from_memory_8bit(cpu.PC+2),read_from_memory_8bit(cpu.PC+1)));
                return 16;
            } cpu.PC += 3; return 12;

        case 0xD0:
            if (!is_C_flag_up()) {
                jump_to_address(extract_adress(read_from_memory_8bit(cpu.PC+2),read_from_memory_8bit(cpu.PC+1)));
                return 16;
            } cpu.PC += 3; return 12;

        case 0xE0: write_into_memory_8bit(extract_adress(0xFF, cpu.C), cpu.A); cpu.PC++; return 8;
        case 0xF0: set_8bit_register('A',read_from_memory_8bit(extract_adress(0xFF, cpu.C))); cpu.PC++; return 8;
        default:
    }
}
uint8_t check_operand_row_collum_3(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: update_increment_16bit_register(&cpu.B, &cpu.C,1,true, false); cpu.PC++; return 8;
        case 0x10: update_increment_16bit_register(&cpu.D, &cpu.E,1,true, false); cpu.PC++; return 8;
        case 0x20: update_increment_16bit_register(&cpu.H, &cpu.L,1, true, false); cpu.PC++; return 8; // load
        case 0x30: cpu.SP++; cpu.PC++; return 8;

        case 0x40: set_8bit_register('B', cpu.E); cpu.PC++; return 4;
        case 0x50: set_8bit_register('D', cpu.E); cpu.PC++; return 4;
        case 0x60: set_8bit_register('H', cpu.E); cpu.PC++; return 4;
        case 0x70: write_into_memory_8bit(extract_adress(cpu.H,cpu.L), cpu.E); cpu.PC++; return 8;

        case 0x80: update_increment_8bit_register(&cpu.A, cpu.E, false, false); cpu.PC++; return 4;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.E, false, false); cpu.PC++; return 4;
        case 0xA0: update_comparator_AND_8bit_register(&cpu.A, cpu.E); cpu.PC++; return 4;
        case 0xB0: update_comparator_OR_8bit_register(&cpu.A, cpu.E); cpu.PC++; return 4;

        case 0xC0: jump_to_address(extract_adress(read_from_memory_8bit(cpu.PC+2),read_from_memory_8bit(cpu.PC+1))); return 16;
        case 0xD0: return 0;
        case 0xE0: return 0;
        case 0xF0: cpu.IME = false; cpu.PC++; return 4;
        default:
    }
}
uint8_t check_operand_row_collum_4(uint8_t opcode) {
    uint8_t temp_var;
    switch (get_opcode_row(opcode)) {
        case 0x00: update_increment_8bit_register(&cpu.B, 1,true,false); cpu.PC++; return 4;
        case 0x10: update_increment_8bit_register(&cpu.D, 1,true,false); cpu.PC++; return 4;  // load HL, n16
        case 0x20: update_increment_8bit_register(&cpu.H, 1,true,false); cpu.PC++; return 4;  // load
        case 0x30:
            temp_var = read_from_memory_8bit(extract_adress(cpu.H,cpu.L));
            update_increment_8bit_register(&temp_var, 1,true,false);
            write_into_memory_8bit(extract_adress(cpu.H,cpu.L), temp_var);
            cpu.PC++; return 12;

        case 0x40: set_8bit_register('B', cpu.H); cpu.PC++; return 4;
        case 0x50: set_8bit_register('D', cpu.H); cpu.PC++; return 4;
        case 0x60: set_8bit_register('H', cpu.H); cpu.PC++; return 4;
        case 0x70: write_into_memory_8bit(extract_adress(cpu.H,cpu.L), cpu.H); cpu.PC++; return 8;

        case 0x80: update_increment_8bit_register(&cpu.A, cpu.H, false, false); cpu.PC++; return 4;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.H, false, false); cpu.PC++; return 4;
        case 0xA0: update_comparator_AND_8bit_register(&cpu.A, cpu.H); cpu.PC++; return 4;
        case 0xB0: update_comparator_OR_8bit_register(&cpu.A, cpu.H); cpu.PC++; return 4;

        case 0xC0:
            if (!is_Z_flag_up()) {
                cpu.PC+=3;
                call_to_address(extract_adress(read_from_memory_8bit(cpu.PC-1),read_from_memory_8bit(cpu.PC-2)));
                return 24;
            } cpu.PC+=3; return 12;
        case 0xD0:
            if (!is_C_flag_up()) {
                cpu.PC+=3;
                call_to_address(extract_adress(read_from_memory_8bit(cpu.PC-1),read_from_memory_8bit(cpu.PC-2)));
                return 24;
            } cpu.PC+=3; return 12;
        case 0xE0: return 0;
        case 0xF0: return 0;
        default:
    }
}
uint8_t check_operand_row_collum_5(uint8_t opcode) {
    uint8_t temp_var;
    switch (get_opcode_row(opcode)) {
        case 0x00: update_decrement_8bit_register(&cpu.B, 1, true, false); cpu.PC++; return 4;
        case 0x10: update_decrement_8bit_register(&cpu.D, 1, true, false); cpu.PC++; return 4;  // load HL, n16
        case 0x20: update_decrement_8bit_register(&cpu.H, 1, true, false); cpu.PC++; return 4;  // load
        case 0x30:
            temp_var = read_from_memory_8bit(extract_adress(cpu.H,cpu.L));
            update_decrement_8bit_register(&temp_var, 1,true,false);
            write_into_memory_8bit(extract_adress(cpu.H,cpu.L), temp_var);
            cpu.PC++; return 12;

        case 0x40: set_8bit_register('B', cpu.L); cpu.PC++; return 4;
        case 0x50: set_8bit_register('D', cpu.L); cpu.PC++; return 4;
        case 0x60: set_8bit_register('H', cpu.L); cpu.PC++; return 4;
        case 0x70: write_into_memory_8bit(extract_adress(cpu.H,cpu.L), cpu.L); cpu.PC++; return 8;

        case 0x80: update_increment_8bit_register(&cpu.A, cpu.L, false, false); cpu.PC++; return 4;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.L, false, false); cpu.PC++; return 4;
        case 0xA0: update_comparator_AND_8bit_register(&cpu.A, cpu.L); cpu.PC++; return 4;
        case 0xB0: update_comparator_OR_8bit_register(&cpu.A, cpu.L); cpu.PC++; return 4;

        case 0xC0: push_into_stack_16bit(cpu.B,cpu.C); cpu.PC++; return 16;
        case 0xD0: push_into_stack_16bit(cpu.D,cpu.E); cpu.PC++; return 16;
        case 0xE0: push_into_stack_16bit(cpu.H,cpu.L); cpu.PC++; return 16;
        case 0xF0: push_into_stack_16bit(cpu.A,cpu.F); cpu.PC++; return 16;
        default:
    }
}
uint8_t check_operand_row_collum_6(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: set_8bit_register('B', read_from_memory_8bit(cpu.PC+1)); cpu.PC+=2; return 8;
        case 0x10: set_8bit_register('D', read_from_memory_8bit(cpu.PC+1)); cpu.PC+=2; return 8;
        case 0x20: set_8bit_register('H', read_from_memory_8bit(cpu.PC+1)); cpu.PC+=2; return 8;
        case 0x30: write_into_memory_8bit(extract_adress(cpu.H, cpu.L), read_from_memory_8bit(cpu.PC+1)); cpu.PC+=2; return 12;

        case 0x40: set_8bit_register('B', read_from_memory_8bit(extract_adress(cpu.H, cpu.L))); cpu.PC++; return 8;
        case 0x50: set_8bit_register('D', read_from_memory_8bit(extract_adress(cpu.H, cpu.L))); cpu.PC++; return 8;
        case 0x60: set_8bit_register('H', read_from_memory_8bit(extract_adress(cpu.H, cpu.L))); cpu.PC++; return 8;
        case 0x70:
            if (cpu.IME) {
                cpu.is_halted = true; cpu.PC++; return 4;
            };
            if (check_if_is_interrupted(true)) {
                cpu.halt_bug = true; cpu.PC++; return 4;
            }
            cpu.only_waiting_for_interrupt_cond = true; cpu.PC++; return 4;

        case 0x80: update_increment_8bit_register(&cpu.A, read_from_memory_8bit(extract_adress(cpu.H, cpu.L)), false, false); cpu.PC++; return 8;
        case 0x90: update_decrement_8bit_register(&cpu.A, read_from_memory_8bit(extract_adress(cpu.H, cpu.L)), false, false); cpu.PC++; return 8;
        case 0xA0: update_comparator_AND_8bit_register(&cpu.A, read_from_memory_8bit(extract_adress(cpu.H, cpu.L))); cpu.PC++; return 8;
        case 0xB0: update_comparator_OR_8bit_register(&cpu.A, read_from_memory_8bit(extract_adress(cpu.H, cpu.L))); cpu.PC++; return 8;

        case 0xC0: update_increment_8bit_register(&cpu.A, read_from_memory_8bit(cpu.PC+1), false, false); cpu.PC+=2; return 8;
        case 0xD0: update_decrement_8bit_register(&cpu.A, read_from_memory_8bit(cpu.PC+1), false, false); cpu.PC+=2; return 4;
        case 0xE0: update_comparator_AND_8bit_register(&cpu.A, read_from_memory_8bit(cpu.PC+1)); cpu.PC+=2; return 8;
        case 0xF0: update_comparator_OR_8bit_register(&cpu.A, read_from_memory_8bit(cpu.PC+1)); cpu.PC+=2; return 8;
        default:
    }
}
uint8_t check_operand_row_collum_7(uint8_t opcode) {
    uint16_t temporary_var_rotation;
    uint8_t adjustment;
    switch (get_opcode_row(opcode)) {
        case 0x00:
            temporary_var_rotation = (uint16_t) cpu.A<<1;
            if (temporary_var_rotation & 0b100000000) {
                temporary_var_rotation |= 0b1;
                set_C_flag_up(true);
            }
            else set_C_flag_up(false);
            cpu.A = (uint8_t)(temporary_var_rotation);
            set_Z_flag_up(false);
            set_H_flag_up(false);
            set_N_flag_up(false);
            cpu.PC++;
            return 4;
        case 0x10:
            temporary_var_rotation = (uint16_t) cpu.A<<1;
            if (is_C_flag_up()) {
                temporary_var_rotation |= 0b1;
                set_C_flag_up(false);
            }
            else temporary_var_rotation |= 0b0;
            if (temporary_var_rotation & 0b100000000) set_C_flag_up(true);
            else set_C_flag_up(false);
            set_Z_flag_up(false);
            set_H_flag_up(false);
            set_N_flag_up(false);
            cpu.A = (uint8_t)(temporary_var_rotation);
            cpu.PC++;
            return 4;
        case 0x20:
            if (is_N_flag_up()) {
                adjustment = 0;
                if (is_H_flag_up()) adjustment+=0x6;
                if (is_C_flag_up()) adjustment+=0x60;
                cpu.A -= adjustment;
                if (cpu.A == 0) set_Z_flag_up(true);
                set_H_flag_up(false);
                cpu.PC++;
                return 4;
            }
            adjustment = 0;
            if (is_H_flag_up() || (cpu.A & 0xF) > 0x9) adjustment+=0x6;
            if (is_C_flag_up() || (cpu.A) > 0x99) {
                adjustment+=0x60;
                set_C_flag_up(true);}
            cpu.A += adjustment;
            if (cpu.A == 0) set_Z_flag_up(true);
            cpu.PC++;
            set_H_flag_up(false);
            return 4;
        case 0x30: set_C_flag_up(true); set_H_flag_up(false); set_N_flag_up(false); cpu.PC++; return 4;

        case 0x40: set_8bit_register('B', cpu.A); cpu.PC++; return 4;
        case 0x50: set_8bit_register('D', cpu.A); cpu.PC++; return 4;
        case 0x60: set_8bit_register('H', cpu.A); cpu.PC++; return 4;
        case 0x70: write_into_memory_8bit(extract_adress(cpu.H,cpu.L), cpu.A); cpu.PC++; return 8;

        case 0x80: update_increment_8bit_register(&cpu.A, cpu.A, false, false); cpu.PC++; return 4;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.A, false, false); cpu.PC++; return 4;
        case 0xA0: update_comparator_AND_8bit_register(&cpu.A, cpu.A); cpu.PC++; return 4;
        case 0xB0: update_comparator_OR_8bit_register(&cpu.A, cpu.A); cpu.PC++; return 4;

        case 0xC0: cpu.PC++; call_to_address(0x00); return 16;
        case 0xD0: cpu.PC++; call_to_address(0x10); return 16;
        case 0xE0: cpu.PC++; call_to_address(0x20); return 16;
        case 0xF0: cpu.PC++; call_to_address(0x30); return 16;
        default:
    }
}
uint8_t check_operand_row_collum_8(uint8_t opcode) {
    uint16_t temp;
    switch (get_opcode_row(opcode)) {
        case 0x00:
            write_into_memory_8bit(extract_adress(read_from_memory_8bit(cpu.PC+2),read_from_memory_8bit(cpu.PC+1)), cpu.SP & 0x00FF);
            write_into_memory_8bit(extract_adress(read_from_memory_8bit(cpu.PC+2),read_from_memory_8bit(cpu.PC+1)) + 1, (uint8_t)((cpu.SP & 0xFF00) >> 8));
            cpu.PC+=3;
            return 20;
        case 0x10:
            jump_relative((int8_t)(read_from_memory_8bit(cpu.PC+1))); return 12;
        case 0x20:
            if (is_Z_flag_up()){jump_relative((int8_t)(read_from_memory_8bit(cpu.PC+1))); return 12;}
            cpu.PC+=2; return 8; cpu.PC+=2;
        case 0x30:
            if (is_C_flag_up()){jump_relative((int8_t)(read_from_memory_8bit(cpu.PC+1)));return 12;}
            cpu.PC+=2; return 8;

        case 0x40: set_8bit_register('C', cpu.B); cpu.PC++; return 4;
        case 0x50: set_8bit_register('E', cpu.B); cpu.PC++; return 4;
        case 0x60: set_8bit_register('L', cpu.B); cpu.PC++; return 4;
        case 0x70: set_8bit_register('A', cpu.B); cpu.PC++; return 4;

        case 0x80: update_increment_8bit_register(&cpu.A, cpu.B, false, true); cpu.PC++; return 4;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.B, false, true); cpu.PC++; return 4;
        case 0xA0: update_comparator_XOR_8bit_register(&cpu.A, cpu.B); cpu.PC++; return 4;
        case 0xB0: update_comparator_CP_8bit_register(cpu.A, cpu.B); cpu.PC++; return 4;

        case 0xC0:
            if (is_Z_flag_up()) {return_to_call_address(); return 20;}
            cpu.PC++; return 8;
        case 0xD0:
            if (is_C_flag_up()) {return_to_call_address(); return 20;}
            cpu.PC++; return 8;
        case 0xE0:
            update_increment_SP_e8((int8_t)read_from_memory_8bit(cpu.PC+1)); cpu.PC+=2; return 16;
        case 0xF0:
            temp = cpu.SP;
            update_increment_SP_e8((int8_t)read_from_memory_8bit(cpu.PC+1));
            set_16bit_register('H', 'L', cpu.SP);
            cpu.SP = temp;cpu.PC+=2; return 12;
        default:
    }
}
uint8_t check_operand_row_collum_9(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: update_increment_16bit_register(&cpu.H,&cpu.L,extract_adress(cpu.B,cpu.C),false,false); cpu.PC++; return 8;
        case 0x10: update_increment_16bit_register(&cpu.H,&cpu.L,extract_adress(cpu.D,cpu.E),false,false); cpu.PC++; return 8;
        case 0x20: update_increment_16bit_register(&cpu.H,&cpu.L,extract_adress(cpu.H,cpu.L),false,false); cpu.PC++; return 8; // load
        case 0x30: update_increment_16bit_register(&cpu.H,&cpu.L, cpu.SP,false,false); cpu.PC++; return 8;

        case 0x40: cpu.PC++; return 4;
        case 0x50: set_8bit_register('E', cpu.C); cpu.PC++; return 4;
        case 0x60: set_8bit_register('L', cpu.C); cpu.PC++; return 4;
        case 0x70: set_8bit_register('A', cpu.C); cpu.PC++; return 4;

        case 0x80: update_increment_8bit_register(&cpu.A, cpu.C, false, true); cpu.PC++; return 4;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.C, false, true); cpu.PC++; return 4;
        case 0xA0: update_comparator_XOR_8bit_register(&cpu.A, cpu.C); cpu.PC++; return 4;
        case 0xB0: update_comparator_CP_8bit_register(cpu.A, cpu.C); cpu.PC++; return 4;

        case 0xC0: return_to_call_address(); return 16;
        case 0xD0: return_to_call_address(); cpu.IME = true; return 16;
        case 0xE0: jump_to_address(extract_adress(cpu.H, cpu.L)); return 4;
        case 0xF0: cpu.SP = extract_adress(cpu.H, cpu.L); cpu.PC++; return 8;
        default:
    }
}
uint8_t check_operand_row_collum_A(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: set_8bit_register('A', read_from_memory_8bit(extract_adress(cpu.B, cpu.C))); cpu.PC++; return 8;
        case 0x10: set_8bit_register('A', read_from_memory_8bit(extract_adress(cpu.D, cpu.E))); cpu.PC++; return 8; // load HL, n16
        case 0x20:
            set_8bit_register('A', read_from_memory_8bit(extract_adress(cpu.H, cpu.L)));
            update_increment_16bit_register(&cpu.H, &cpu.L, 1, true, false); cpu.PC++; return 8;
        case 0x30:
            set_8bit_register('A', read_from_memory_8bit(extract_adress(cpu.H, cpu.L)));
            update_decrement_16bit_register(&cpu.H, &cpu.L, 1); cpu.PC++; return 8;

        case 0x40: set_8bit_register('C', cpu.D); cpu.PC++; return 4;
        case 0x50: set_8bit_register('E', cpu.D); cpu.PC++; return 4;
        case 0x60: set_8bit_register('L', cpu.D); cpu.PC++; return 4;
        case 0x70: set_8bit_register('A', cpu.D); cpu.PC++; return 4;

        case 0x80: update_increment_8bit_register(&cpu.A, cpu.D, false, true); cpu.PC++; return 4;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.D, false, true); cpu.PC++; return 4;
        case 0xA0: update_comparator_XOR_8bit_register(&cpu.A, cpu.D); cpu.PC++; return 4;
        case 0xB0: update_comparator_CP_8bit_register(cpu.A, cpu.D); cpu.PC++; return 4;

        case 0xC0:
            if (is_Z_flag_up()) {
                jump_to_address(extract_adress(read_from_memory_8bit(cpu.PC+2), read_from_memory_8bit(cpu.PC+1)));
                return 16;
            }
            cpu.PC+=3; return 12;
        case 0xD0:
            if (is_C_flag_up()) {
                jump_to_address(extract_adress(read_from_memory_8bit(cpu.PC+2), read_from_memory_8bit(cpu.PC+1)));
                return 16;
            }
            cpu.PC+=3; return 12;
        case 0xE0: write_into_memory_8bit(extract_adress(read_from_memory_8bit(cpu.PC+2),read_from_memory_8bit(cpu.PC+1)),cpu.A); cpu.PC+=3; return 16;
        case 0xF0: set_8bit_register('A', read_from_memory_8bit(extract_adress(read_from_memory_8bit(cpu.PC+2),read_from_memory_8bit(cpu.PC+1)))); cpu.PC+=3; return 16;
        default:
    }
}
uint8_t check_operand_row_collum_B(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: update_decrement_16bit_register(&cpu.B, &cpu.C, 1); cpu.PC++; return 8;
        case 0x10: update_decrement_16bit_register(&cpu.D, &cpu.E, 1); cpu.PC++; return 8; // load HL, n16
        case 0x20: update_decrement_16bit_register(&cpu.H, &cpu.L, 1); cpu.PC++; return 8; // load
        case 0x30: cpu.SP--; cpu.PC++; return 8;

        case 0x40: set_8bit_register('C', cpu.E); cpu.PC++; return 4;
        case 0x50: cpu.PC++; return 4;
        case 0x60: set_8bit_register('L', cpu.E); cpu.PC++; return 4;
        case 0x70: set_8bit_register('A', cpu.E); cpu.PC++; return 4;

        case 0x80: update_increment_8bit_register(&cpu.A, cpu.E, false, true); cpu.PC++; return 4;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.E, false, true); cpu.PC++; return 4;
        case 0xA0: update_comparator_XOR_8bit_register(&cpu.A, cpu.E); cpu.PC++; return 4;
        case 0xB0: update_comparator_CP_8bit_register(cpu.A, cpu.E); cpu.PC++; return 4;

        case 0xC0: return check_operand_collumn_CB_PREFIX(read_from_memory_8bit(cpu.PC +1));
        case 0xD0: return 0;
        case 0xE0: return 0;
        case 0xF0: cpu.enable_interrupt = true; cpu.PC++; return 4;  //TODO: cond_enable EI
        default:
    }
}
uint8_t check_operand_row_collum_C(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: update_increment_8bit_register(&cpu.C, 1, true, false); cpu.PC++; return 4;
        case 0x10: update_increment_8bit_register(&cpu.E, 1, true, false); cpu.PC++; return 4; // load HL, n16
        case 0x20: update_increment_8bit_register(&cpu.L, 1, true, false); cpu.PC++; return 4; // load
        case 0x30: update_increment_8bit_register(&cpu.A, 1, true, false); cpu.PC++; return 4;

        case 0x40: set_8bit_register('C', cpu.H); cpu.PC++; return 4;
        case 0x50: set_8bit_register('E', cpu.H); cpu.PC++; return 4;
        case 0x60: set_8bit_register('L', cpu.H); cpu.PC++; return 4;
        case 0x70: set_8bit_register('A', cpu.H); cpu.PC++; return 4;

        case 0x80: update_increment_8bit_register(&cpu.A, cpu.H, false, true); cpu.PC++; return 4;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.H, false, true); cpu.PC++; return 4;
        case 0xA0: update_comparator_XOR_8bit_register(&cpu.A, cpu.H); cpu.PC++; return 4;
        case 0xB0: update_comparator_CP_8bit_register(cpu.A, cpu.H); cpu.PC++; return 4;

        case 0xC0:
            if (is_Z_flag_up()) {
                cpu.PC+=3;
                call_to_address(extract_adress(read_from_memory_8bit(cpu.PC-1),read_from_memory_8bit(cpu.PC-2)));
                return 24;
            } cpu.PC+=3; return 12;
        case 0xD0:
            if (is_C_flag_up()) {
                cpu.PC+=3;
                call_to_address(extract_adress(read_from_memory_8bit(cpu.PC-1),read_from_memory_8bit(cpu.PC-2)));
                return 24;
            } cpu.PC+=3; return 12;
        case 0xE0: return 0;
        case 0xF0: return 0;
    }
}
uint8_t check_operand_row_collum_D(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: update_decrement_8bit_register(&cpu.C, 1, true, false); cpu.PC++; return 4;
        case 0x10: update_decrement_8bit_register(&cpu.E, 1, true, false); cpu.PC++; return 4; // load HL, n16
        case 0x20: update_decrement_8bit_register(&cpu.L, 1, true, false); cpu.PC++; return 4; // load
        case 0x30: update_decrement_8bit_register(&cpu.A, 1, true, false); cpu.PC++; return 4;

        case 0x40: set_8bit_register('C', cpu.L); cpu.PC++; return 4;
        case 0x50: set_8bit_register('E', cpu.L); cpu.PC++; return 4;
        case 0x60: set_8bit_register('L', cpu.L); cpu.PC++; return 4;
        case 0x70: set_8bit_register('A', cpu.L); cpu.PC++; return 4;

        case 0x80: update_increment_8bit_register(&cpu.A, cpu.L, false, true); cpu.PC++; return 4;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.L, false, true); cpu.PC++; return 4;
        case 0xA0: update_comparator_XOR_8bit_register(&cpu.A, cpu.L); cpu.PC++; return 4;
        case 0xB0: update_comparator_CP_8bit_register(cpu.A, cpu.L); cpu.PC++; return 4;

        case 0xC0: cpu.PC+=3; call_to_address(extract_adress(read_from_memory_8bit(cpu.PC-1),read_from_memory_8bit(cpu.PC-2))); return 24;
        case 0xD0: exit(1);
        case 0xE0: exit(1);
        case 0xF0: exit(1);
    }
}
uint8_t check_operand_row_collum_E(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: set_8bit_register('C', read_from_memory_8bit(cpu.PC+1)); cpu.PC+=2; return 8;
        case 0x10: set_8bit_register('E', read_from_memory_8bit(cpu.PC+1)); cpu.PC+=2; return 8; // load HL, n16
        case 0x20: set_8bit_register('L', read_from_memory_8bit(cpu.PC+1)); cpu.PC+=2; return 8; // load
        case 0x30: set_8bit_register('A', read_from_memory_8bit(cpu.PC+1)); cpu.PC+=2; return 8;

        case 0x40: set_8bit_register('C', read_from_memory_8bit(extract_adress(cpu.H,cpu.L))); cpu.PC++; return 8;
        case 0x50: set_8bit_register('E', read_from_memory_8bit(extract_adress(cpu.H,cpu.L))); cpu.PC++; return 8;
        case 0x60: set_8bit_register('L', read_from_memory_8bit(extract_adress(cpu.H,cpu.L))); cpu.PC++; return 8;
        case 0x70: set_8bit_register('A', read_from_memory_8bit(extract_adress(cpu.H,cpu.L))); cpu.PC++; return 8;

        case 0x80: update_increment_8bit_register(&cpu.A, read_from_memory_8bit(extract_adress(cpu.H, cpu.L)), false, true); cpu.PC++; return 4;
        case 0x90: update_decrement_8bit_register(&cpu.A, read_from_memory_8bit(extract_adress(cpu.H, cpu.L)), false, true); cpu.PC++; return 4;
        case 0xA0: update_comparator_XOR_8bit_register(&cpu.A, read_from_memory_8bit(extract_adress(cpu.H, cpu.L))); cpu.PC++; return 4;
        case 0xB0: update_comparator_CP_8bit_register(cpu.A, read_from_memory_8bit(extract_adress(cpu.H, cpu.L))); cpu.PC++; return 4;

        case 0xC0: update_increment_8bit_register(&cpu.A, read_from_memory_8bit(cpu.PC+1), false, true); cpu.PC+=2; return 8;
        case 0xD0: update_decrement_8bit_register(&cpu.A, read_from_memory_8bit(cpu.PC+1), false, true); cpu.PC+=2; return 8;
        case 0xE0: update_comparator_XOR_8bit_register(&cpu.A, read_from_memory_8bit(cpu.PC+1)); cpu.PC+=2; return 8;
        case 0xF0: update_comparator_CP_8bit_register(cpu.A, read_from_memory_8bit(cpu.PC+1)); cpu.PC+=2; return 8;
    }
}
uint8_t check_operand_row_collum_F(uint8_t opcode) {
    uint8_t temporary_var_rotation;
    switch (get_opcode_row(opcode)) {
        case 0x00:
            temporary_var_rotation = cpu.A >> 1;
            if (cpu.A & 0b00000001) {
                temporary_var_rotation |= 0b10000000;
                set_C_flag_up(true);
            }
            else set_C_flag_up(false);
            cpu.A = (uint8_t)(temporary_var_rotation);
            set_Z_flag_up(false);
            set_H_flag_up(false);
            set_N_flag_up(false);
            cpu.PC++;
            return 4;
        case 0x10:
            temporary_var_rotation = (uint16_t) cpu.A>>1;
            if (is_C_flag_up()) {
                temporary_var_rotation |= 0b10000000;
                set_C_flag_up(false);
            }
            if (cpu.A & 0b00000001) {
                set_C_flag_up(true);
            }
            else set_C_flag_up(false);
            set_Z_flag_up(false);
            set_H_flag_up(false);
            set_N_flag_up(false);
            cpu.A = (uint8_t)(temporary_var_rotation);
            cpu.PC++;
            return 4;
        case 0x20: cpu.A = ~cpu.A; set_N_flag_up(true); set_H_flag_up(true); cpu.PC++; return 4;  // load
        case 0x30: set_N_flag_up(false); set_H_flag_up(false); set_C_flag_up(!is_C_flag_up()); cpu.PC++; return 4;

        case 0x40: set_8bit_register('C', cpu.A); cpu.PC++; return 4;
        case 0x50: set_8bit_register('E', cpu.A); cpu.PC++; return 4;
        case 0x60: set_8bit_register('L', cpu.A); cpu.PC++; return 4;
        case 0x70: cpu.PC++; return 4;

        case 0x80: update_increment_8bit_register(&cpu.A, cpu.A, false, true); cpu.PC++; return 4;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.A, false, true); cpu.PC++; return 4;
        case 0xA0: update_comparator_XOR_8bit_register(&cpu.A, cpu.A); set_Z_flag_up(true); set_N_flag_up(false); set_H_flag_up(false); set_C_flag_up(false); cpu.PC++; return 4;
        case 0xB0: update_comparator_CP_8bit_register(cpu.A, cpu.A); set_Z_flag_up(true); set_N_flag_up(true) ; set_H_flag_up(false); set_C_flag_up(false); cpu.PC++; return 4;

        case 0xC0: cpu.PC++; call_to_address(0x08); return 16;
        case 0xD0: cpu.PC++; call_to_address(0x18); return 16;
        case 0xE0: cpu.PC++; call_to_address(0x28); return 16;
        case 0xF0: cpu.PC++; call_to_address(0x38); return 16;
    }
}
uint8_t check_operand_collumn(uint8_t opcode) {
    switch (get_opcode_collum(opcode)) {
        case 0x00: return check_operand_row_collum_0(opcode);
        case 0x01: return check_operand_row_collum_1(opcode);
        case 0x02: return check_operand_row_collum_2(opcode);
        case 0x03: return check_operand_row_collum_3(opcode);
        case 0x04: return check_operand_row_collum_4(opcode);
        case 0x05: return check_operand_row_collum_5(opcode);
        case 0x06: return check_operand_row_collum_6(opcode);
        case 0x07: return check_operand_row_collum_7(opcode);
        case 0x08: return check_operand_row_collum_8(opcode);
        case 0x09: return check_operand_row_collum_9(opcode);
        case 0x0A: return check_operand_row_collum_A(opcode);
        case 0x0B: return check_operand_row_collum_B(opcode);
        case 0x0C: return check_operand_row_collum_C(opcode);
        case 0x0D: return check_operand_row_collum_D(opcode);
        case 0x0E: return check_operand_row_collum_E(opcode);
        case 0x0F: return check_operand_row_collum_F(opcode);
    }
}
void incrementar_ciclos(uint8_t ciclos) {
    if (check_LCDC(7))ppu.contador_ciclos += ciclos;
    cpu.contador_ciclos_div += ciclos;
    if (read_from_memory_8bit(0xFF07) & 0b00000100) {
        cpu.contador_ciclos_tima += ciclos;
    }
}
void clear_registers() {
    cpu.A = 0;
    cpu.B = 0;
    cpu.C = 0;
    cpu.D = 0;
    cpu.E = 0;
    cpu.H = 0;
    cpu.L = 0;
    cpu.F = 0b00000000;
    cpu.SP = 0xFFFE;
    cpu.PC = 0x0100;
    cpu.IME = false;
    cpu.tima_is_on = true;
    cpu.halt_bug = false;
    cpu.only_waiting_for_interrupt_cond = false;
    cpu.is_halted = false;
    cpu.contador_ciclos = 0;
    cpu.contador_ciclos_div = 0;
    cpu.contador_ciclos_tima = 0;
    ppu.contador_ciclos=0;
    ppu.current_LY=0;
    ppu.current_mode=0;
    ppu.executou_modo_0 = false;
    ppu.executou_modo_1 = false;
    ppu.executou_modo_2 = false;
    ppu.executou_modo_3 = false;
    ppu.executou_ly_lyc = false;
    ppu.window_top_left.x=0;
    ppu.window_top_left.y=0;
    ppu.current_LY_from_window = 0;
    cpu.DMA_transfer_pending = false;
    cpu.DMA_transfer_curr_addr = 0;
    cpu.DMA_transfer_limit_addr = 0;
    cpu.DMA_transfer_OAM_addr = 0xFE00;
    write_into_memory_8bit(0xFF40, 0x91);
}
/*
 * Renderização ainda não implementada.
 *
 * A partir daqui ficam apenas os pontos de entrada que você vai reescrever.
 * O controle de ciclos, modos, LY, LYC, VBlank e interrupções continua abaixo.
 */
void SDL_SetColor0(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 155, 188, 15, 255); // #9BBC0F
}
void SDL_SetColor1(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 139, 172, 15, 255); // #8BAC0F
}
void SDL_SetColor2(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 48, 98, 48, 255); // #306230
}
void SDL_SetColor3(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 15, 56, 15, 255); // #0F380F
}
void render_to_lcd() {
    for (int i =0; i<144;i++) {
        for (int j = 0; j<160;j++) {
            switch (ppu.LCDscreen[i][j]) {
                case 0b00: SDL_SetColor0(renderer); break;
                case 0b01: SDL_SetColor1(renderer); break;
                case 0b10: SDL_SetColor2(renderer); break;
                case 0b11: SDL_SetColor3(renderer); break;
            }
            SDL_RenderDrawPoint(renderer, j , i);
        }
    }
}

// primera etapa, busca na OAM da scanline atual
/*.
Byte   Attributes/Fags
       
Attributes Priority Y�fip X�fip DMG�paette Bank CGB�paette
• Priority 0  No, 1  BG and Window coor indices 1 are drawn over this OBJ
• Y fip 0  Norma, 1  Entire OBJ is verticay mirrored
• X fip 0  Norma, 1  Entire OBJ is horizontay mirrored
• DMG paette Non CGB Mode ony 0  OBP, 1  OBP1
• Bank CGB Mode Ony 0  Fetch tie from VRAM bank , 1  Fetch tie from
VRAM bank 1
• CGB paette CGB Mode Ony Which of OBP7 to use
*/
void oam_scan() {
    //int contador_de_obj = 0;
    uint16_t endereco_sprite = 0xFE00;
    uint8_t current_ly = get_curr_LY();
    uint8_t curr_obj_y;
    uint8_t curr_obj_x;
    bool is_8x16;
    ppu.sprite_count = 0;
    for (int i = 0; i < 10; i++) {
        ppu.sprite_bank[i].attributes = 0;
        ppu.sprite_bank[i].tile_ID_8x16 = 0;
        ppu.sprite_bank[i].tile_ID = 0;
        ppu.sprite_bank[i].coord_x = 0;
        ppu.sprite_bank[i].coord_y = 0;
    }
    for (int i = 0; i < 160 && ppu.sprite_count<10; i+=4) {
        curr_obj_y = read_from_memory_8bit(endereco_sprite+i);
        curr_obj_x = read_from_memory_8bit(endereco_sprite+i+1);
        is_8x16 = check_LCDC(2);
        int16_t result = ((int16_t)current_ly) - (((int16_t)curr_obj_y) - 16);
        if (1){
            if (is_8x16) {
                //printf("teste\n");
                if (result < 16 && result >= 0) {
                    ppu.sprite_bank[ppu.sprite_count].coord_y      =  curr_obj_y;
                    ppu.sprite_bank[ppu.sprite_count].coord_x      =  curr_obj_x;
                    ppu.sprite_bank[ppu.sprite_count].tile_ID      =  read_from_memory_8bit(endereco_sprite+i+2) & 0b11111110;
                    ppu.sprite_bank[ppu.sprite_count].tile_ID_8x16 =  ppu.sprite_bank[ppu.sprite_count].tile_ID + 1;
                    ppu.sprite_bank[ppu.sprite_count].attributes   =  read_from_memory_8bit(endereco_sprite+i+3);
                    ppu.sprite_count++;
                }
            }
            else {
                if (result < 8 && result >= 0) {
                    ppu.sprite_bank[ppu.sprite_count].coord_y      =  curr_obj_y;
                    ppu.sprite_bank[ppu.sprite_count].coord_x      =  curr_obj_x;
                    ppu.sprite_bank[ppu.sprite_count].tile_ID      =  read_from_memory_8bit(endereco_sprite+i+2);
                    ppu.sprite_bank[ppu.sprite_count].attributes   =  read_from_memory_8bit(endereco_sprite+i+3);
                    ppu.sprite_count++;
                }
            }
        }
    }
}

void draw_sprite_pixels(int16_t start_index, bool prioridade_BG, bool **pixel_in_X_axis_claimed, int j, bool *pode_desenhar, uint8_t color_byte1, uint8_t color_byte2, uint8_t paleta, uint8_t curr_LY, bool x_flip) {
    if ((start_index >= 0)) {
        uint8_t low_bit, high_bit;
        if (!((*pixel_in_X_axis_claimed)[start_index])) {
            *pode_desenhar = true;
        }
        if (*pode_desenhar){
            if (x_flip) {
                low_bit  = (color_byte1 << (7-j) & 0b10000000) >> 7;
                high_bit = (color_byte2 << (7-j) & 0b10000000) >> 6;
            }
            else {
                low_bit  = (color_byte1 << (j) & 0b10000000) >> 7;
                high_bit = (color_byte2 << (j) & 0b10000000) >> 6;
            }
            uint8_t color = (high_bit|low_bit);//printf("color = %u\n", color);
            if (color != 0) {
                (*pixel_in_X_axis_claimed)[start_index] = true;
                uint8_t shade = (paleta) ? (read_from_memory_8bit(0xFF49) >> (color * 2)) & 0x03 :  (read_from_memory_8bit(0xFF48) >> (color * 2)) & 0x03;
                if (!(prioridade_BG))ppu.LCDscreen[curr_LY][start_index] = shade;
                else {
                    if (ppu.LCDscreen_pixel_color[curr_LY][start_index] == 0) ppu.LCDscreen[curr_LY][start_index] = shade;
                }
            }
        }
    }
}

void render_sprites_in_line() {
    uint16_t default_addr = 0x8000;
    uint16_t tile_addr;
    int16_t x_coord;
    int16_t y_coord;
    uint8_t curr_LY = get_curr_LY(), curr_tile_ly;
    uint8_t color_byte1, color_byte2;
    bool prioridade_BG;
    bool y_flip;
    bool x_flip;
    bool paleta;
    bool pode_desenhar;
    uint8_t sprite_tile[8];
    int16_t start_index;
    uint8_t tile_pixel_x;
    uint8_t color, high_bit, low_bit, shade;
    bool *pixel_in_X_axis_claimed = NULL;
    pixel_in_X_axis_claimed = malloc(sizeof(bool) * 160);
    for (int i = 0; i < 160; i++) pixel_in_X_axis_claimed[i] = false;
    for (int i = 0; i < ppu.sprite_count; i++) {
        pode_desenhar = false;
        prioridade_BG = (ppu.sprite_bank[i].attributes & 0b10000000);
        y_flip = (ppu.sprite_bank[i].attributes & 0b01000000);
        x_flip = (ppu.sprite_bank[i].attributes & 0b00100000);
        paleta = (ppu.sprite_bank[i].attributes & 0b00010000);
        x_coord = ppu.sprite_bank[i].coord_x -  8;
        y_coord = ppu.sprite_bank[i].coord_y - 16;
        curr_tile_ly = curr_LY - y_coord;
        tile_addr = (y_flip)? default_addr + ((ppu.sprite_bank[i].tile_ID * 16) + (16 - (2*(curr_tile_ly +1)))) : default_addr + ((ppu.sprite_bank[i].tile_ID * 16) + ((2*(curr_tile_ly))));
        // ppu.LCDscreen_pixel_color[curr_ly][i] = color_bit;
        // ppu.LCDscreen[curr_ly][i] = shade;

        if (x_coord < 0) {
            color_byte1 = read_from_memory_8bit(tile_addr);
            color_byte2 = read_from_memory_8bit(tile_addr+1);
            for (int j = 0; j < 8; j++) {
                //
                start_index = x_coord + j;
                draw_sprite_pixels(start_index, prioridade_BG, &pixel_in_X_axis_claimed, j, &pode_desenhar, color_byte1, color_byte2, paleta, curr_LY, x_flip);
            }
        }
        else if (x_coord>=160) {
            start_index = x_coord;
            color_byte1 = read_from_memory_8bit(tile_addr);
            color_byte2 = read_from_memory_8bit(tile_addr+1);
            for (int j = 0; (j < 8 && start_index < 168); j++) {
                start_index = x_coord + j;
                draw_sprite_pixels(start_index, prioridade_BG, &pixel_in_X_axis_claimed, j, &pode_desenhar, color_byte1, color_byte2, paleta, curr_LY, x_flip);
            }
        }
        else {
            color_byte1 = read_from_memory_8bit(tile_addr);
            color_byte2 = read_from_memory_8bit(tile_addr+1);
            for (int j = 0; j < 8 ; j++) {
                start_index = x_coord + j;
                draw_sprite_pixels(start_index, prioridade_BG, &pixel_in_X_axis_claimed, j, &pode_desenhar, color_byte1, color_byte2, paleta, curr_LY, x_flip);
            }
        }
    }
    free(pixel_in_X_axis_claimed);
}

// sprites 8x16
// tiles vem em seguida
void render_sprites_in_line_8x16() {
    uint16_t default_addr = 0x8000;
    uint16_t tile_addr;
    int16_t x_coord;
    int16_t y_coord;
    int16_t curr_LY = get_curr_LY(), curr_tile_ly;
    uint8_t color_byte1, color_byte2;
    bool prioridade_BG;
    bool y_flip;
    bool x_flip;
    bool paleta;
    bool pode_desenhar;
    int16_t start_index;
    bool *pixel_in_X_axis_claimed = NULL;
    pixel_in_X_axis_claimed = malloc(sizeof(bool) * 160);
    for (int i = 0; i < 160; i++) pixel_in_X_axis_claimed[i] = false;
    for (int i = 0; i < ppu.sprite_count; i++) {
        pode_desenhar = false;
        prioridade_BG = (ppu.sprite_bank[i].attributes & 0b10000000);
        y_flip = (ppu.sprite_bank[i].attributes & 0b01000000);
        x_flip = (ppu.sprite_bank[i].attributes & 0b00100000);
        paleta = (ppu.sprite_bank[i].attributes & 0b00010000);
        x_coord = ppu.sprite_bank[i].coord_x -  8;
        y_coord = ppu.sprite_bank[i].coord_y - 16;
        curr_tile_ly = curr_LY - y_coord;
        if (curr_tile_ly < 8) {
            tile_addr = (y_flip)? default_addr + ((ppu.sprite_bank[i].tile_ID_8x16 * 16) + (16 - (2*(curr_tile_ly+1))))  : default_addr + ((ppu.sprite_bank[i].tile_ID * 16) + ((2*curr_tile_ly)));
        }
        else {
            curr_tile_ly-=8;
            tile_addr = (y_flip)? default_addr + ((ppu.sprite_bank[i].tile_ID * 16) + (16 - (2*(curr_tile_ly+1)))) : default_addr + ((ppu.sprite_bank[i].tile_ID_8x16 * 16) + ((2*curr_tile_ly)));
        }
        // ppu.LCDscreen_pixel_color[curr_ly][i] = color_bit;
        // ppu.LCDscreen[curr_ly][i] = shade;
        //printf("teste!\n");
        if (x_coord < 0) {
            color_byte1 = read_from_memory_8bit(tile_addr);
            color_byte2 = read_from_memory_8bit(tile_addr+1);
            for (int j = 0; j < 8; j++) {
                start_index =(int16_t) x_coord + j;
                draw_sprite_pixels(start_index, prioridade_BG, &pixel_in_X_axis_claimed, j, &pode_desenhar, color_byte1, color_byte2, paleta, curr_LY, x_flip);
            }
        }
        else if (x_coord>=160) {
            start_index = x_coord;
            color_byte1 = read_from_memory_8bit(tile_addr);
            color_byte2 = read_from_memory_8bit(tile_addr+1);
            for (int j = 0; (j < 8 && start_index < 168); j++) {
                start_index = x_coord + j;
                draw_sprite_pixels(start_index, prioridade_BG, &pixel_in_X_axis_claimed, j, &pode_desenhar, color_byte1, color_byte2, paleta, curr_LY, x_flip);
            }
        }
        else {
            color_byte1 = read_from_memory_8bit(tile_addr);
            color_byte2 = read_from_memory_8bit(tile_addr+1);
            for (int j = 0; j < 8 ; j++) {
                start_index = x_coord + j;
                draw_sprite_pixels(start_index, prioridade_BG, &pixel_in_X_axis_claimed, j, &pode_desenhar, color_byte1, color_byte2, paleta, curr_LY, x_flip);
            }
        }
    }
    free(pixel_in_X_axis_claimed);
}


void render_scanline() {
    bool cond_window_on = check_LCDC(5);
    bool cond_update_window = false;

    // seleciona qual banco de tile map usar
    // true  = 9C00-9FFF
    // false = 9800-9BFF
    bool tile_map_area_window = check_LCDC(6);

    // seleciona qual banco de tile map usar
    // true  = 9C00-9FFF
    // false = 9800-9BFF
    bool tile_map_area_background = check_LCDC(3);

    // seleciona qual banco de tile data usar
    // true  = 8000-8FFF
    // false = 8800-97FF
    bool tile_data_shared = check_LCDC(4);

    uint8_t curr_ly = get_curr_LY();

    // offshift do scroll
    uint8_t curr_SCY = read_from_memory_8bit(0xFF42), curr_SCX = read_from_memory_8bit(0xFF43);

    // inicio da window
    uint8_t curr_WY = read_from_memory_8bit(0xFF4A);
    int16_t curr_WX = read_from_memory_8bit(0xFF4B) -7;

    // offset para achar o primeiro byte do tile (1 tile = 16 bytes)
    uint8_t curr_tile_ID;
    uint16_t curr_tile_ID_addr;
    uint16_t byte_tile_data_0_addr,byte_tile_data_1_addr;
    uint8_t curr_LY_tile;
    uint8_t color_bit;
    uint8_t BGP = read_from_memory_8bit(0xFF47);
    uint8_t shade;
    if (check_LCDC(0)) {
        for (int i = 0; i<160;i++) {
            // bloco window
            curr_LY_tile = ppu.current_LY_from_window%8 * 2;
            if (cond_window_on && (curr_ly >= curr_WY) && (i >= curr_WX)) {
                cond_update_window = true;
                // ** INICIO CAPTURA DA TILE ID **
                if (tile_map_area_window) {// true usa 0x9c00
                    curr_tile_ID_addr = ((i-curr_WX)/8) + (ppu.current_LY_from_window/8)*32;
                    curr_tile_ID = read_from_memory_8bit( 0x9C00 + curr_tile_ID_addr);
                }
                else {// false usa 0x9800
                    curr_tile_ID_addr = ((i-curr_WX)/8) + (ppu.current_LY_from_window/8)*32;
                    curr_tile_ID = read_from_memory_8bit( 0x9800 + curr_tile_ID_addr);
                }
                // ** FINAL CAPTURA DA TILE ID **
                // ** INICIO DA CAPTURA DA TILE DATA **
                // BASICAMENTE VAI SER O ADDR DO TILE DATA + O TILE ID SÓ MUDANDO NO CASO DO SIGNED ID
                if (tile_data_shared) {
                    byte_tile_data_0_addr = 0x8000 + curr_LY_tile + curr_tile_ID * 16;
                    byte_tile_data_1_addr = 0x8000 + curr_LY_tile + curr_tile_ID * 16 + 1;
                }
                else {
                    byte_tile_data_0_addr = 0x9000 + curr_LY_tile + ((int8_t) curr_tile_ID * 16);
                    byte_tile_data_1_addr = 0x9000 + curr_LY_tile + ((int8_t) curr_tile_ID * 16) + 1;
                }
                // pra selecionar o bit é so fazer um i % 8 (com i sendo o index atual)
                // (dessa forma ele traz o bit equivalente no grid 8x8 do tile)
                color_bit = ((read_from_memory_8bit(byte_tile_data_0_addr) >> (7 - ((i - curr_WX) % 8))) & 1) | (((read_from_memory_8bit(byte_tile_data_1_addr) >> (7 - ((i - curr_WX) % 8))) & 1) << 1);
                shade = (BGP >> (color_bit*2)) & 0x03;
            }
            // bloco BG
            else {
                curr_LY_tile = ((curr_ly + curr_SCY) % 8) * 2;
                // ** INICIO CAPTURA DA TILE ID **
                if (tile_map_area_background) {// true usa 0x9c00
                    curr_tile_ID_addr = ((((curr_ly + curr_SCY)%256)/8)*32) + (((i + curr_SCX)%256)/8);
                    curr_tile_ID = read_from_memory_8bit( 0x9C00 + curr_tile_ID_addr);
                }
                else {// false usa 0x9800
                    curr_tile_ID_addr = ((((curr_ly + curr_SCY)%256)/8)*32) + (((i + curr_SCX)%256)/8);
                    curr_tile_ID = read_from_memory_8bit( 0x9800 + curr_tile_ID_addr);
                }
                // ** FINAL CAPTURA DA TILE DATA **
                // ** INICIO DA CAPTURA DA TILE DATA **
                if (tile_data_shared) {
                    byte_tile_data_0_addr = 0x8000 + curr_LY_tile + curr_tile_ID * 16;
                    byte_tile_data_1_addr = 0x8000 + curr_LY_tile + curr_tile_ID * 16 + 1;
                }
                else {
                    byte_tile_data_0_addr = 0x9000 + curr_LY_tile + ((int8_t) curr_tile_ID)*16;
                    byte_tile_data_1_addr = 0x9000 + curr_LY_tile + ((int8_t) curr_tile_ID)*16 + 1;
                }
                // pra selecionar o bit é so fazer um i % 8 (com i sendo o index atual)
                // (dessa forma ele traz o bit equivalente no grid 8x8 do tile)
                color_bit = ((read_from_memory_8bit(byte_tile_data_0_addr) >> (7 - ((i + curr_SCX) % 8))) & 1) | (((read_from_memory_8bit(byte_tile_data_1_addr) >> (7 - ((i + curr_SCX) % 8))) & 1) << 1);
                shade = (BGP >> (color_bit*2)) & 0x03;
            }
            ppu.LCDscreen_pixel_color[curr_ly][i] = color_bit;
            ppu.LCDscreen[curr_ly][i] = shade;
        }
        if (cond_update_window) ppu.current_LY_from_window++;
    }
}

void ppu_cycles_Verify() {
    /*
     * Fecha a scanline atual.
     *
     * Este bloco fica aqui em cima. O tratamento separado de LY == 153
     * que existia mais abaixo foi removido para LY não ser incrementado
     * duas vezes.
     */
    if (!check_LCDC(7)) {
        return;
    }
    if (ppu.contador_ciclos >= (80 + 172 + 204)) {
        ppu.contador_ciclos -= 456;
        ppu.executou_modo_0 = false;
        ppu.executou_modo_2 = false;
        ppu.executou_modo_3 = false;
        //vblank_start_joypad = false;
        ppu.executou_ly_lyc = false;
        if (get_curr_LY() == 153) {
            cpu.frame_end = SDL_GetPerformanceCounter();
            cpu.time_elapsed = ((double)(cpu.frame_end - cpu.frame_start) * 1000.0)/(SDL_GetPerformanceFrequency()*1.0);
            emulation_delay_left = emulation_delay_target - cpu.time_elapsed;
            if (emulation_delay_left > 0 && emulation_delay_left < emulation_delay_target) SDL_Delay(emulation_delay_left);
            ppu.current_LY_from_window = 0;
            ppu.executou_modo_1 = false;

        }
        increment_LY();
    }

    /*
     * Coincidência LY == LYC.
     * A flag impede pedir repetidamente a mesma interrupção durante a linha.
     */
    if (compare_lyc_ly() && !ppu.executou_ly_lyc) {
        if (get_STAT_mode(6)) {
            set_interrupt(1, true);
        }
        ppu.executou_ly_lyc = true;
    }

    if (get_curr_LY() < 144) {
        /*
         * Mode 2: OAM Scan.
         */
        if (ppu.contador_ciclos < 80 && !ppu.executou_modo_2) {
            cpu.frame_start = SDL_GetPerformanceCounter();
            set_ppu_mode(2);
            if (get_STAT_mode(5)) {
                set_interrupt(1, true);
            }
            ppu.executou_modo_2 = true;
        }

        /*
         * Mode 3: Pixel Transfer
         *
         * A renderização ainda não entra aqui
         */
        else if (80 <= ppu.contador_ciclos && ppu.contador_ciclos < (80 + 172) && !ppu.executou_modo_3) {
            oam_scan();
            set_ppu_mode(3);
            render_scanline();
            if (check_LCDC(1)){
                (check_LCDC(2))? render_sprites_in_line_8x16() : render_sprites_in_line();
            }
            ppu.executou_modo_3 = true;
        }

        /*
         * Mode 0: HBlank
         */
        else if (
            (80 + 172) <= ppu.contador_ciclos && ppu.contador_ciclos < (80 + 172 + 204) && !ppu.executou_modo_0) {
            set_ppu_mode(0);

            if (get_STAT_mode(3)) {
                set_interrupt(1, true);
            }

            ppu.executou_modo_0 = true;
        }
    }
    else {
        /*
         * Mode 1: VBlank, LY 144 até 153.
         *
         * A interrupção VBlank e a interrupção STAT de Mode 1 são pedidas
         * apenas uma vez, na primeira entrada em VBlank.
         */
        if (!ppu.executou_modo_1) {
            set_ppu_mode(1);
            SDL_RenderClear(renderer);
            render_to_lcd();
            SDL_RenderPresent(renderer);
            vblank_start_joypad = true;
            if (get_STAT_mode(4)) {
                set_interrupt(1, true);
            }
            set_interrupt(0, true);
        }
        ppu.executou_modo_1 = true;
    }
}

void DMA_transfer_Verify(uint8_t ciclos) {
    // 1 byte = 4 ciclos
    if (!cpu.DMA_transfer_pending) return;
    for (int i = 0; i < ciclos/4; i++) {
        write_into_memory_8bit_DMA(cpu.DMA_transfer_OAM_addr,read_from_memory_8bit(cpu.DMA_transfer_curr_addr));
        cpu.DMA_transfer_OAM_addr++;
        cpu.DMA_transfer_curr_addr++;
        if (cpu.DMA_transfer_curr_addr == cpu.DMA_transfer_limit_addr) {
            cpu.DMA_transfer_curr_addr = 0x0;
            cpu.DMA_transfer_limit_addr = 0x0;
            cpu.DMA_transfer_OAM_addr = 0xFE00;
            cpu.DMA_transfer_pending = false;
            return;
        }
    }
}

int main(int argc, char*argv[]) {
    if (SDL_Init(SDL_INIT_EVERYTHING)!=0) {
        perror("SDL nao inicializou");
        SDL_Quit();
    }
    // SDL_CreateWindowAndRenderer(160, 288, SDL_WINDOW_SHOWN, &screen2, &renderer_OAM);
    // if (screen2 == NULL) {
    //     SDL_DestroyWindow(screen2);
    // }
    SDL_CreateWindowAndRenderer(160, 144, SDL_WINDOW_SHOWN, &screen, &renderer);
    if (screen == NULL) {
        SDL_DestroyWindow(screen);
    }
    screenSurface = SDL_GetWindowSurface(screen);
    // screenSurface2 = SDL_GetWindowSurface(screen2);
    // SDL_RenderPresent(renderer_OAM);
    // SDL_FillRect(screenSurface2, NULL, 0x000000);
    uint16_t temporary_PC_addr = 0x0000;
    clear_registers();
    char* path_rom = argv[1];
    if (!cond_debug) start_game(path_rom);
    //printf("Gameboy ROM size: %d\n", memory.game_rom_lenght);
    int curr_operation = 0;
    if (cond_debug) {
        while (curr_operation<nmbr_tests){
            load_memory(tests_extra[curr_operation]);
            clear_registers();
            printf("[> Current Test: %03d <]_____[> Operation name: %20s <]____________________________________________________________________________________________________.\n",curr_operation,tests_extra[curr_operation].name);
            printf("|                                                                                                                                                                          |\n");
            while (read_from_memory_8bit(cpu.PC)!=0x10) {
                //if (curr_operation==0) printf("\n\n0x%04x\n\n",read_from_memory_8bit(0xC000));
                //usleep(2000);
                printf("|_________________________________________________________________________________________________________________________________________________________________________|\n");

                printf("| Current instruction: [>  %02x  <] | Register A: %02x | Register B: %02x | Register C: %02x | Register D: %02x | Register E: %02x | Register F: %02x | Register L: %02x | Register H: %02x |\n"
                                 "| Current PC:          [> %04x <] | Flag Z: %02x     | Flag N: %02x     | Flag H: %02x     | Flag C: %02x     |                |                |                |                |\n", read_from_memory_8bit(cpu.PC), cpu.A,cpu.B,cpu.C,cpu.D,cpu.E,cpu.F,cpu.L,cpu.H,cpu.PC, is_Z_flag_up(), is_N_flag_up(), is_H_flag_up(), is_C_flag_up());
                // TODO(CPU): revisar o encadeamento abaixo; o segundo if possui um else próprio,
                // TODO(CPU): então a CPU pode passar pelo bloco HALT e ainda executar uma instrução na mesma iteração.
                if (cpu.is_halted) {
                    incrementar_ciclos(4);
                    cpu.only_waiting_for_interrupt_cond = !check_if_is_interrupted(true);
                    check_cycle_counter();
                    cpu.is_halted = !check_if_is_interrupted(false);
                }
                else if (cpu.only_waiting_for_interrupt_cond){
                    incrementar_ciclos(4);
                    check_cycle_counter();
                    cpu.only_waiting_for_interrupt_cond = !check_if_is_interrupted(true);
                    //sleep(2);
                }
                else {
                    if (cpu.halt_bug) temporary_PC_addr = cpu.PC;
                    incrementar_ciclos(check_operand_collumn(read_from_memory_8bit(cpu.PC)));
                    if (cpu.halt_bug) {
                        cpu.PC = temporary_PC_addr;
                        cpu.halt_bug = false;
                    }
                    check_cycle_counter();
                    if (cpu.IME) cpu.is_halted = check_if_is_interrupted(true);
                    if (cpu.enable_interrupt) {cpu.IME = true; cpu.enable_interrupt = false;}
                }
            }

            printf("|_________________________________________________________________________________________________________________________________________________________________________|\n");
            printf("|[> Results <]____________________________________________________________________________________________________________________________________________________________|\n");
            printf("| Current instruction: [>  %02x  <] | Register A: %02x | Register B: %02x | Register C: %02x | Register D: %02x | Register E: %02x | Register F: %02x | Register L: %02x | Register H: %02x |\n"
                         "| Current PC:          [> %04x <] | Flag Z: %02x     | Flag N: %02x     | Flag H: %02x     | Flag C: %02x     |                |                |                |               |\n", read_from_memory_8bit(cpu.PC), cpu.A,cpu.B,cpu.C,cpu.D,cpu.E,cpu.F,cpu.L,cpu.H,cpu.PC, is_Z_flag_up(), is_N_flag_up(), is_H_flag_up(), is_C_flag_up());
            printf("|_________________________________________________________________________________________________________________________________________________________________________|\n");
            printf("| Cycles_DIV => %3d | Cycles_TIMA => %3d                                                                                                                                  |\n", cpu.contador_ciclos_div, cpu.contador_ciclos_tima);
            printf("!_________________________________________________________________________________________________________________________________________________________________________!\n\n");
            curr_operation++;
        }
    }
    else {
        printf("\n\n[> Current ROM: %15s <]\n",path_rom);
        bool cond_go = true;
        //int cond = 0;
        //long int count = 0;
        //int count_ppu = 0;
        //memory.IO[0] = 0b00001111;
        static uint8_t old_state = 0xFF;
        static uint8_t old_timer = 0xFF;
        SDL_Event event;
        write_into_memory_8bit(0xFF00, 0b00101111);
        while (1) {
            if (vblank_start_joypad) {
                vblank_start_joypad = false;
                //printf("FF8C = %02X | FFC0 = %02X | FF91 = %02X\n ", read_from_memory_8bit(0xFF8C),read_from_memory_8bit(0xFFc0), read_from_memory_8bit(0xFF91));
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_KEYUP) {
                        switch (event.key.keysym.sym) {
                            case SDLK_LEFT:
                                on_left_button = false; break;
                                // <- esquerda
                            case SDLK_RIGHT:
                                on_right_button = false; break;
                                // direita ->
                            case SDLK_UP:
                                on_up_button = false; break;
                            case SDLK_DOWN:
                                on_down_button = false; break;
                            case SDLK_a:
                                on_A_button = false; break;
                                // botão A
                            case SDLK_b:
                                on_B_button = false; break;
                                // botão B
                            case SDLK_z:
                                on_select_button = false; break;
                                // botão select
                            case SDLK_x:
                                on_start_button = false; break;
                                //botão start
                        }
                    }
                    if (event.type == SDL_KEYDOWN) {
                        switch (event.key.keysym.sym) {
                            case SDLK_LEFT:
                                on_left_button = true; break;
                                // <- esquerda
                            case SDLK_RIGHT:
                                on_right_button = true; break;
                                // direita ->
                            case SDLK_UP:
                                on_up_button = true; break;
                            case SDLK_DOWN:
                                on_down_button = true; break;

                            case SDLK_a:
                                on_A_button = true; break;
                                // botão A
                            case SDLK_b:
                                on_B_button = true; break;
                                // botão B
                            case SDLK_z:
                                on_select_button = true; break;
                                // botão select
                            case SDLK_x:
                                on_start_button = true; break;
                                //botão start
                        }
                    }
                }
            }

            //
            //uint8_t state = read_from_memory_8bit(0xFFE1);
            //uint8_t timer = read_from_memory_8bit(0xFFA6);
            //if (state != old_state || timer != old_timer) {
            //printf("STATE=%02X TIMER=%02X PC=%04X LY=%02X FF85=%02X\n",state,timer,cpu.PC,read_from_memory_8bit(0xFF44),read_from_memory_8bit(0xFF85));old_state = state;old_timer = timer;
            //}
            //render_map(false);

            //printf("| Current instruction: [>  %02x  <] | Register A: %02x | Register B: %02x | Register C: %02x | Register D: %02x | Register E: %02x | Register F: %02x | Register L: %02x | Register H: %02x |\n"
            //             "| Current PC:          [> %04x <] | Flag Z: %02x     | Flag N: %02x     | Flag H: %02x     | Flag C: %02x     |                |                |                |                |\n", read_from_memory_8bit(cpu.PC), cpu.A,cpu.B,cpu.C,cpu.D,cpu.E,cpu.F,cpu.L,cpu.H,cpu.PC, is_Z_flag_up(), is_N_flag_up(), is_H_flag_up(), is_C_flag_up());
            //printf("|_________________________________________________________________________________________________________________________________________________________________________|\n");
            //u1p(20000);
            //uint8_t current_state = read_from_memory_8bit(0xFFE1);
            //cond++;
            //printf("curr instruction: %02x\n", read_from_memory_8bit(cpu.PC));
            if (cpu.is_halted) {
                if (cond_start_vblank && !cond_ja_foi_vblank) {
                    set_interrupt(0,true);
                    cond_ja_foi_vblank = true;
                }
                if (cond_start_STAT && !cond_ja_foi_STAT) {
                    set_interrupt(1,true);
                    cond_ja_foi_STAT = true;
                }
                incrementar_ciclos(0x4);
                DMA_transfer_Verify(0x4);
                check_cycle_counter();ppu_cycles_Verify();
                cpu.is_halted = !check_if_is_interrupted(false);
            }
            else if (cpu.only_waiting_for_interrupt_cond){
                if (cond_start_vblank && !cond_ja_foi_vblank) {
                    set_interrupt(0,true);
                    cond_ja_foi_vblank = true;
                }
                if (cond_start_STAT && !cond_ja_foi_STAT) {
                    set_interrupt(1,true);
                    cond_ja_foi_STAT = true;
                }
                incrementar_ciclos(0x4);
                DMA_transfer_Verify(0x4);
                check_cycle_counter();
                ppu_cycles_Verify();
                cpu.only_waiting_for_interrupt_cond = !check_if_is_interrupted(true);
            }
            else {
                if (cond_start_vblank && !cond_ja_foi_vblank) {
                    set_interrupt(0,true);
                    cond_ja_foi_vblank = true;

                }
                if (cond_start_STAT && !cond_ja_foi_STAT) {
                    set_interrupt(1,true);
                    cond_ja_foi_STAT = true;
                }
                if (cpu.halt_bug) temporary_PC_addr = cpu.PC;
                uint8_t nbr =check_operand_collumn(read_from_memory_8bit(cpu.PC) );
                //printf("%d | ",nbr);
                incrementar_ciclos(nbr);
                DMA_transfer_Verify(nbr);
                if (cpu.halt_bug) {
                    cpu.PC = temporary_PC_addr;
                    cpu.halt_bug = false;
                }
                check_cycle_counter();
                ppu_cycles_Verify();
                if (cpu.IME) cpu.is_halted = check_if_is_interrupted(true);
                if (cpu.enable_interrupt) {cpu.IME = true; cpu.enable_interrupt = false;}
            }
        }
        SDL_DestroyWindow(screen);
        SDL_Quit();
    }
}
