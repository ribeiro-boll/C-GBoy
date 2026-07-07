#include <stdio.h>
#include <string.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct Reg {
    bool is_halted;
    bool tima_is_on;
    bool IME;

    uint8_t lower_byte;
    uint8_t upper_byte;

    uint8_t curr_operation;

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

    int contador_ciclos;
    int contador_ciclos_div;
    int contador_ciclos_tima;
} GB_reg;

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

bool cond_jump_PC = false;

bool cond_wait_8bit_var_A = false;
bool cond_wait_8bit_var_B = false;
bool cond_wait_8bit_var_C = false;
bool cond_wait_8bit_var_D = false;
bool cond_wait_8bit_var_E = false;
bool cond_wait_8bit_var_H = false;
bool cond_wait_8bit_var_L = false;

bool cond_wait_16bit_var_BC = false; // espera 2 bytes e executa
bool cond_wait_16bit_var_DE = false; // espera 2 bytes e executa
bool cond_wait_16bit_var_HL = false; // espera 2 bytes e executa
bool cond_wait_16bit_var_SP = false; // espera 2 bytes e executa
bool cond_wait_8bit_var_HL = false;
bool cond_wait_16bit_var = false;
bool cond_wait_double_8bit_var = false;
bool cond_wait_double_16bit_var = false;
bool cond_wait_8bit_and_16bit_var = false;
bool cond_wait_16bit_and_8bit_var = false;


uint8_t program[] = {
    0x21, 0x00, 0xC0, // LD HL, $C000
    0x3E, 0x04,       // LD A, $04
    0x06, 0x03,       // LD B, $03
    0x80,             // ADD A, B
    0x77,             // LD [HL], A
    0x3C,             // INC A
    0x23,             // INC HL
    0x77,             // LD [HL], A
    0xC3, 0x0C, 0x01  // JP $010C
};

// r8 Any N 8-bit register (A, B, C, D, E, H, L)
// r16 Any N 16 bit register
// n8 8-bit integer constant | signed or unsigned, -128/255
// n16 16-bit integer constant | signed or unsigned, -32768/65535
// e8 signed offset | -128/127
// u3 signed short 8 bit int | -128/127


GB_reg cpu;
GB_Memory memory;

void check_cycle_counter() {
    while (cpu.contador_ciclos_div >=256) {
        cpu.contador_ciclos_div -= 256;
        memory.IO[0x04] +=1; // div
    }
    if (memory.IO[0x07] & 0b00000100) {
        switch (memory.IO[0x07]) { // TIMA
            case 0b00000101: while (cpu.contador_ciclos_tima >=   16) {
                if (memory.IO[0x05] == 0xFF) {
                    memory.IO[0x05] = memory.IO[0x06];
                    cpu.contador_ciclos_tima -= 16;
                    memory.IO[0x0F] |= 0b00000100;
                }
                else {
                    cpu.contador_ciclos_tima -= 16;
                    memory.IO[0x05]+=1;
                }
            } break;
            case 0b00000110: if (cpu.contador_ciclos_tima >=   64) {
                if (memory.IO[0x05] == 0xFF) {
                    memory.IO[0x05] = memory.IO[0x06];
                    cpu.contador_ciclos_tima -= 64;
                    memory.IO[0x0F] |= 0b00000100;
                }
                else {
                    cpu.contador_ciclos_tima -= 64;
                    memory.IO[0x05]+=1;
                }
            }   break;
            case 0b00000111: if (cpu.contador_ciclos_tima >=  256) {
                if (memory.IO[0x05] == 0xFF) {
                    memory.IO[0x05] = memory.IO[0x06];
                    cpu.contador_ciclos_tima -= 256 ;
                    memory.IO[0x0F] |= 0b00000100;
                }
                else {
                    cpu.contador_ciclos_tima -= 256;
                    memory.IO[0x05]+=1;
                }
            } break;
            case 0b00000100: if (cpu.contador_ciclos_tima >= 1024) {
                if (memory.IO[0x05] == 0xFF) {
                    memory.IO[0x05] = memory.IO[0x06];
                    cpu.contador_ciclos_tima -= 1024;
                    memory.IO[0x0F] |= 0b00000100;
                }
                else {
                    cpu.contador_ciclos_tima -= 1024;
                    memory.IO[0x05]+=1;
                }
            } break;
        }

    }
}
/*
bit 2    → timer ligado/desligado
bits 1-0 → frequência do TIMA
00 → TIMA incrementa a cada 1024 ciclos
01 → TIMA incrementa a cada 16 ciclos
10 → TIMA incrementa a cada 64 ciclos
11 → TIMA incrementa a cada 256 ciclos
*/
void incrementar_ciclos(uint8_t ciclos) {
    cpu.contador_ciclos_div+= ciclos;
    cpu.contador_ciclos_tima+= ciclos;
}
void push_into_stack_16bit(uint8_t upper_byte, uint8_t lower_byte) {
    cpu.SP--;
    memory.HRAM[cpu.SP] = upper_byte;
    cpu.SP--;
    memory.HRAM[cpu.SP] = lower_byte;
}

void pop_from_stack_to_register_16bit(uint8_t *upper_byte, uint8_t *lower_byte, bool cond_AF) {
    *lower_byte = memory.HRAM[cpu.SP];
    cpu.SP++;
    if (cond_AF) {
        *lower_byte = *lower_byte & 0xF0;
    }
    *upper_byte = memory.HRAM[cpu.SP];
    cpu.SP++;
}

uint16_t pop_from_stack_for_emulator_use_16bit() {
    uint8_t lower_byte, upper_byte;
    lower_byte = memory.HRAM[cpu.SP];
    cpu.SP++;
    upper_byte = memory.HRAM[cpu.SP];
    cpu.SP++;
    return ((uint16_t)(upper_byte) << 8) & (lower_byte);
}

//______________________________________
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

void set_Z_flag(bool up_or_down) {
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
void jump_to_address(uint16_t address) {
    cpu.PC = address;
}
void jump_relative(int8_t value) {
    cpu.PC += ((int8_t)value)+2;
}
void call_to_address(uint16_t address) {
    push_into_stack_16bit((uint8_t)(cpu.PC & 0xFF00) >> 8, (uint8_t)(cpu.PC & 0x00FF));
    jump_to_address(address);
}
void return_to_call_address() {
    cpu.PC = pop_from_stack_for_emulator_use_16bit();
}
//______________________________________
void start_game(char* game_adress) {
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
    if (address >= 0xFF00 && 0xFF7F >= address) return memory.IO[address - 0xFF00];
    if (address >= 0xFF80 && 0xFFFE >= address) return memory.HRAM[address - 0xFF80];
    if (0xFFFF == address) return memory.IE;
    return 0;
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
uint16_t extract_adress(uint8_t upper_byte, uint8_t lower_byte) {
    uint16_t address = ((uint16_t)(upper_byte << 8) | (lower_byte));
    return address;
}
//______________________________________
void update_comparator_AND_8bit_register(uint8_t *target, uint8_t variable) {
    ((*target & variable) == 0) ? set_Z_flag(true) : set_Z_flag(false);
    set_N_flag_up(false);
    set_H_flag_up(true);
    set_C_flag_up(false);
    *target = *target & variable;
}
void update_comparator_OR_8bit_register(uint8_t *target, uint8_t variable) {
    ((*target | variable) == 0) ? set_Z_flag(true) : set_Z_flag(false);
    set_N_flag_up(false);
    set_H_flag_up(false);
    set_C_flag_up(false);
    *target = *target | variable;
}
void update_comparator_XOR_8bit_register(uint8_t *target, uint8_t variable) {
    ((*target ^ variable) == 0) ? set_Z_flag(true) : set_Z_flag(false);
    set_N_flag_up(false);
    set_H_flag_up(false);
    set_C_flag_up(false);
    *target = *target ^ variable;
}
void update_comparator_CP_8bit_register(uint8_t *target, uint8_t variable) {
    ((*target - variable) == 0) ? set_Z_flag(true) : set_Z_flag(false);
    set_N_flag_up(true);
    (((*target & 0x0F) < ((variable & 0x0F)))) ? set_H_flag_up(true) : set_H_flag_up(false);
    (((*target & 0xFF) < ((variable & 0xFF)))) ? set_C_flag_up(true) : set_C_flag_up(false);
}
//______________________________________
void update_decrement_16bit_register(uint8_t *upper_byte, uint8_t *lower_byte, uint8_t decrement) {
    uint16_t target_hex = extract_adress(*upper_byte, *lower_byte);
    uint16_t result = (uint16_t)(((uint16_t)target_hex - decrement) % (0x10000));
    *upper_byte = (uint8_t)((result & 0xFF00) >> 8);
    *lower_byte = (uint8_t)((result & 0xFF));
}
void update_increment_SP_e8(int8_t e8) {
    uint16_t old_sp = cpu.SP;
    cpu.SP = (uint16_t)(cpu.SP + e8);
    set_Z_flag(false);
    set_N_flag_up(false);
    (((old_sp & 0x0F) + (e8 & 0x0F)) > 0x0F) ? set_H_flag_up(true) : set_H_flag_up(false);
    (((old_sp & 0xFF) + (e8 & 0xFF)) > 0xFF) ? set_C_flag_up(true) : set_C_flag_up(false);
}
void update_increment_16bit_register(uint8_t *upper_byte, uint8_t *lower_byte, uint16_t increment, bool cond_inc, bool carry_cond) {
    uint16_t target_hex = extract_adress(*upper_byte, *lower_byte);
    uint8_t carry = 0;
    if (carry_cond) {
        if (is_Z_flag_up())
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
}
void update_decrement_8bit_register(uint8_t *target, uint8_t decrement, bool cond_dec, bool carry_cond) {
    uint8_t old_value = *target;
    uint8_t carry = 0;
    if (carry_cond) {
        if (is_C_flag_up())
            carry = 1;
    }
    *target = (uint8_t)((uint16_t)(*target) - decrement - carry);
    set_N_flag_up(true);

    (*target == 0)? set_Z_flag(true) : set_Z_flag(false);
    ((old_value & 0x0F) < ((decrement & 0x0F) + carry)) ? set_H_flag_up(true) : set_H_flag_up(false);
    if (old_value < (decrement + carry) && !cond_dec)
        set_C_flag_up(true);
    else if (!cond_dec)
        set_C_flag_up(false);
}
void update_increment_8bit_register(uint8_t *target, uint8_t increment, bool cond_inc, bool carry_cond) {
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
    (*target == 0) ? set_Z_flag(true) : set_Z_flag(false);
    ((first+second + carry) & 0x10) ? set_H_flag_up(true) : set_H_flag_up(false);
    if (result && !cond_inc)
        set_C_flag_up(true);
    else if (!cond_inc)
        set_C_flag_up(false);
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
//____________________________________________________
void write_into_memory_8bit(uint16_t address, uint8_t variable) {
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
    if (address >= 0xA000 && 0xBFFF >= address) memory.cartRAM[address - 0xA000] = variable;
    if (address >= 0xC000 && 0xDFFF >= address) memory.WRAM[address - 0xC000] = variable;
    if (address >= 0xFE00 && 0xFE9F >= address) memory.OAM[address - 0xFE00] = variable;
    if (address >= 0xFF00 && 0xFF7F >= address) memory.IO[address - 0xFF00] = variable;
    if (address >= 0xFF80 && 0xFFFE >= address) memory.HRAM[address - 0xFF80] = variable;
    if (address == 0xFFFF) memory.IE = variable;
}
//____________________________________________________
void set_16bit_register(char letter_double_register_high_byte, char letter_double_register_lower_byte, uint16_t variable) {
    if (letter_double_register_high_byte == 'B' && letter_double_register_lower_byte == 'C') {
        cpu.B = ((uint8_t)(variable & 0xFF00)>>8);
        cpu.C = ((uint8_t)(variable & 0x00FF));
    }
    else if (letter_double_register_high_byte == 'D' && letter_double_register_lower_byte == 'E') {
        cpu.D = ((uint8_t)(variable & 0xFF00)>>8);
        cpu.E = ((uint8_t)(variable & 0x00FF));
    }
    else if (letter_double_register_high_byte == 'H' && letter_double_register_lower_byte == 'L') {
        cpu.H = ((uint8_t)(variable & 0xFF00)>>8);
        cpu.L = ((uint8_t)(variable & 0x00FF));
    }
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

bool check_if_interrupted() {
    if (cpu.IME && (memory.IO[0x0F] & 0b00000001) && (memory.IE & 0b00000001)) { // IME, IF, IE
        memory.IO[0x0F] &= 0b11111110;
        cpu.IME = false;
        call_to_address(0x0040);
        return false;
    }
    if (cpu.IME && (memory.IO[0x0F] & 0b00000010) && (memory.IE & 0b00000010)) { // IME, IF, IE
        memory.IO[0x0F] &= 0b11111101;
        cpu.IME = false;
        call_to_address(0x0048);
        return false;
    }
    if (cpu.IME && (memory.IO[0x0F] & 0b00000100) && (memory.IE & 0b00000100)) { // IME, IF, IE
        memory.IO[0x0F] &= 0b11111011;
        cpu.IME = false;
        call_to_address(0x0050);
        return false;
    }
    if (cpu.IME && (memory.IO[0x0F] & 0b00001000) && (memory.IE & 0b00001000)) { // IME, IF, IE
        memory.IO[0x0F] &= 0b11110111;
        cpu.IME = false;
        call_to_address(0x0058);
        return false;
    }
    if (cpu.IME && (memory.IO[0x0F] & 0b00010000) && (memory.IE & 0b00010000)) { // IME, IF, IE
        memory.IO[0x0F] &= 0b11101111;
        cpu.IME = false;
        call_to_address(0x0060);
        return false;
    }
    return true;
}

//____________________________________________________
uint8_t check_operand_row_collum_0(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: cpu.PC++; return 4; break;
        case 0x10: cpu.is_halted = true; cpu.PC+=2; return 4;
        case 0x20:
            if (!is_Z_flag_up()) {
                jump_relative(memory.game_rom[cpu.PC+1]);
                return 12;
            } cpu.PC += 2; return 8;
        case 0x30:
            if (!is_C_flag_up()) {
                jump_relative(memory.game_rom[cpu.PC+1]);
                return 12;
            } cpu.PC += 2; return 8;
        case 0x40: cpu.PC++; return 4; break;
        case 0x50: set_8bit_register('D',cpu.B); cpu.PC++; return 4; break;
        case 0x60: set_8bit_register('H', cpu.B); cpu.PC++; return 4; break;
        case 0x70: write_into_memory_8bit(extract_adress(cpu.H,cpu.L), cpu.B); cpu.PC++; return 8; break;
        case 0x80: update_increment_8bit_register(&cpu.A, cpu.B, false, false); cpu.PC++; return 4; break;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.B, false, false); cpu.PC++; return 4; break;
        case 0xA0: update_comparator_AND_8bit_register(&cpu.A, cpu.B); cpu.PC++; return 4; break;
        case 0xB0: update_comparator_OR_8bit_register(&cpu.A, cpu.B); cpu.PC++; return 4; break;
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
        case 0xE0: write_into_memory_8bit(extract_adress(0xFF, memory.game_rom[cpu.PC +1]), cpu.A); cpu.PC+=2; return 12; break;
        case 0xF0: set_8bit_register('A',read_from_memory_8bit(extract_adress(0xFF, memory.game_rom[cpu.PC +1]))); cpu.PC+=2; return 12; break;
        default: break;
    }
}

uint8_t check_operand_row_collum_1(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: set_16bit_register('B','C',extract_adress(memory.game_rom[cpu.PC +1],memory.game_rom[cpu.PC +2])); cpu.PC+=3; return 12; break;
        case 0x10: set_16bit_register('D','E',extract_adress(memory.game_rom[cpu.PC +1],memory.game_rom[cpu.PC +2])); cpu.PC+=3; return 12; break; // load HL, n16
        case 0x20: set_16bit_register('H','L',extract_adress(memory.game_rom[cpu.PC +1],memory.game_rom[cpu.PC +2])); cpu.PC+=3; return 12; break; // load
        case 0x30: cpu.SP = extract_adress(memory.game_rom[cpu.PC +1],memory.game_rom[cpu.PC +2]); cpu.PC+=3; return 12; break;
        case 0x40: set_8bit_register('B', cpu.C); cpu.SP+=1; return 4; break;
        case 0x50: set_8bit_register('D', cpu.C); cpu.SP+=1; return 4;break;
        case 0x60: set_8bit_register('H', cpu.C); cpu.SP+=1; return 4;break;
        case 0x70: write_into_memory_8bit(extract_adress(cpu.H,cpu.L), cpu.C); cpu.PC++; return 8; break;
        case 0x80: update_increment_8bit_register(&cpu.A, cpu.C, false, false); cpu.PC++; return 4; break;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.C, false, false); cpu.PC++; return 4; break;
        case 0xA0: update_comparator_AND_8bit_register(&cpu.A, cpu.C); cpu.PC++; return 4; break;
        case 0xB0: update_comparator_OR_8bit_register(&cpu.A, cpu.C); cpu.PC++; return 4; break;
        case 0xC0: pop_from_stack_to_register_16bit(&cpu.B,&cpu.C,false); cpu.PC++;return 12; break;
        case 0xD0: pop_from_stack_to_register_16bit(&cpu.D,&cpu.E,false); cpu.PC++;return 12; break;
        case 0xE0: pop_from_stack_to_register_16bit(&cpu.H,&cpu.L,false); cpu.PC++;return 12; break;
        case 0xF0: pop_from_stack_to_register_16bit(&cpu.A,&cpu.F,true); cpu.PC++;return 12; break;
        default: break;
    }
}

uint8_t check_operand_row_collum_2(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: write_into_memory_8bit(extract_adress(cpu.B,cpu.C), cpu.A); cpu.PC++; return 8; break;
        case 0x10: write_into_memory_8bit(extract_adress(cpu.D,cpu.E), cpu.A); cpu.PC++; return 8; break;
        case 0x20:
            update_increment_16bit_register(&cpu.H, &cpu.L, 1, true, false);
            write_into_memory_8bit(extract_adress(cpu.H,cpu.L), cpu.A); cpu.PC++; return 8;break;
        case 0x30:
            update_decrement_16bit_register(&cpu.H, &cpu.L, 1);
            write_into_memory_8bit(extract_adress(cpu.H,cpu.L), cpu.A); cpu.PC++; return 8;break;
        case 0x40: set_8bit_register('B', cpu.D); cpu.PC++; return 4; break;
        case 0x50: cpu.PC++; return 4; break;
        case 0x60: set_8bit_register('H', cpu.D); cpu.PC++; return 4;break;
        case 0x70: write_into_memory_8bit(extract_adress(cpu.H, cpu.L), cpu.D); cpu.PC++; return 8;break;
        case 0x80: update_increment_8bit_register(&cpu.A, cpu.D, false, false); cpu.PC++; return 4; break;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.D, false, false); cpu.PC++; return 4; break;
        case 0xA0: update_comparator_AND_8bit_register(&cpu.A, cpu.D); cpu.PC++; return 4; break;
        case 0xB0: update_comparator_OR_8bit_register(&cpu.A, cpu.D); cpu.PC++; return 4; break;
        case 0xC0:
            if (!is_Z_flag_up()) {
                jump_to_address(extract_adress(memory.game_rom[cpu.PC+1],memory.game_rom[cpu.PC+2]));
                return 16;
            } cpu.PC += 3; return 12;
            break;
        case 0xD0:
            if (!is_C_flag_up()) {
                jump_to_address(extract_adress(memory.game_rom[cpu.PC+1],memory.game_rom[cpu.PC+2]));
                return 16;
            } cpu.PC += 3; return 12;
            break;
        case 0xE0: write_into_memory_8bit(extract_adress(0xFF, cpu.C), cpu.A); cpu.PC+=1; return 8; break;
        case 0xF0: set_8bit_register('A',read_from_memory_8bit(extract_adress(0xFF, cpu.C))); cpu.PC+=1; return 8; break;
        default: break;
    }
}


uint8_t check_operand_row_collum_3(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: update_increment_16bit_register(&cpu.B, &cpu.C,1,true, false); cpu.PC++; return 8; break;
        case 0x10: update_increment_16bit_register(&cpu.D, &cpu.E,1,true, false); cpu.PC++; return 8;break;
        case 0x20: update_increment_16bit_register(&cpu.H,&cpu.L,1, true, false); cpu.PC++; return 8;break; // load
        case 0x30: cpu.SP++; cpu.PC++; return 8;
        case 0x40: set_8bit_register('B', cpu.E); cpu.SP+=1; return 4; break;
        case 0x50: set_8bit_register('D', cpu.E); cpu.SP+=1; return 4;break;
        case 0x60: set_8bit_register('H', cpu.E); cpu.SP+=1; return 4;break;
        case 0x70: write_into_memory_8bit(extract_adress(cpu.H,cpu.L), cpu.E); cpu.PC++; return 8; break;
        case 0x80: update_increment_8bit_register(&cpu.A, cpu.E, false, false); cpu.PC++; return 4; break;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.E, false, false); cpu.PC++; return 4; break;
        case 0xA0: update_comparator_AND_8bit_register(&cpu.A, cpu.E); cpu.PC++; return 4; break;
        case 0xB0: update_comparator_OR_8bit_register(&cpu.A, cpu.E); cpu.PC++; return 4; break;
        case 0xC0: jump_to_address(extract_adress(memory.game_rom[cpu.PC+1],memory.game_rom[cpu.PC+2])); return 16; break;
        case 0xD0: return 0; break;
        case 0xE0: return 0; break;
        case 0xF0: cpu.IME = false; cpu.PC++; return 4; break;
        default: break;
    }
}


uint8_t check_operand_row_collum_4(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: update_increment_8bit_register(&cpu.B, 1,true,false); cpu.PC++; return 4; break;
        case 0x10: update_increment_8bit_register(&cpu.D, 1,true,false); cpu.PC++; return 4; break; // load HL, n16
        case 0x20: update_increment_8bit_register(&cpu.H, 1,true,false); cpu.PC++; return 4; break; // load
        case 0x30: update_increment_8bit_register(&memory.game_rom[extract_adress(cpu.H,cpu.L)], 1,true,false); cpu.PC++; return 12; break;
        case 0x40: set_8bit_register('B', cpu.H); cpu.SP+=1; return 4; break;
        case 0x50: set_8bit_register('D', cpu.H); cpu.SP+=1; return 4;break;
        case 0x60: set_8bit_register('H', cpu.H); cpu.SP+=1; return 4;break;
        case 0x70: write_into_memory_8bit(extract_adress(cpu.H,cpu.L), cpu.H); cpu.PC++; return 8; break;
        case 0x80: update_increment_8bit_register(&cpu.A, cpu.H, false, false); cpu.PC++; return 4; break;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.H, false, false); cpu.PC++; return 4; break;
        case 0xA0: update_comparator_AND_8bit_register(&cpu.A, cpu.H); cpu.PC++; return 4; break;
        case 0xB0: update_comparator_OR_8bit_register(&cpu.A, cpu.H); cpu.PC++; return 4; break;
        case 0xC0:
            if (!is_Z_flag_up()) {
                call_to_address(extract_adress(memory.game_rom[cpu.PC+1],memory.game_rom[cpu.PC+2]));
                return 24;
            } cpu.PC+=3; return 12;
        case 0xD0:
            if (!is_C_flag_up()) {
                call_to_address(extract_adress(memory.game_rom[cpu.PC+1],memory.game_rom[cpu.PC+2]));
                return 24;
            } cpu.PC+=3; return 12;
        case 0xE0: return 0; break;
        case 0xF0: return 0; break;
        default: break;
    }
}


uint8_t check_operand_row_collum_5(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: update_decrement_8bit_register(&cpu.B, 1, true, false); cpu.PC++; return 4; break;
        case 0x10: update_decrement_8bit_register(&cpu.D, 1, true, false); cpu.PC++; return 4; break; // load HL, n16
        case 0x20: update_decrement_8bit_register(&cpu.H, 1, true, false); cpu.PC++; return 4; break; // load
        case 0x30: update_decrement_8bit_register(&memory.game_rom[extract_adress(cpu.H,cpu.L)], 1, true,false); cpu.PC++; return 12; break;
        case 0x40: set_8bit_register('B', cpu.L); cpu.SP+=1; return 4; break;
        case 0x50: set_8bit_register('D', cpu.L); cpu.SP+=1; return 4;break;
        case 0x60: set_8bit_register('H', cpu.L); cpu.SP+=1; return 4;break;
        case 0x70: write_into_memory_8bit(extract_adress(cpu.H,cpu.L), cpu.L); cpu.PC++; return 8; break;
        case 0x80: update_increment_8bit_register(&cpu.A, cpu.L, false, false); cpu.PC++; return 4; break;
        case 0x90: update_decrement_8bit_register(&cpu.A, cpu.L, false, false); cpu.PC++; return 4; break;
        case 0xA0: update_comparator_AND_8bit_register(&cpu.A, cpu.L); cpu.PC++; return 4; break;
        case 0xB0: update_comparator_OR_8bit_register(&cpu.A, cpu.L); cpu.PC++; return 4; break;
        case 0xC0: push_into_stack_16bit(cpu.B,cpu.C); cpu.PC++; return 16; break;
        case 0xD0: push_into_stack_16bit(cpu.D,cpu.E); cpu.PC++; return 16; break;
        case 0xE0: push_into_stack_16bit(cpu.H,cpu.L); cpu.PC++; return 16; break;
        case 0xF0: push_into_stack_16bit(cpu.A,cpu.F); cpu.PC++; return 16; break;
        default: break;
    }
}


uint8_t check_operand_row_collum_6(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: set_8bit_register('B', memory.game_rom[cpu.PC+1]); cpu.PC+=2; return 8; break;
        case 0x10: set_8bit_register('D', memory.game_rom[cpu.PC+1]); cpu.PC+=2; return 8; break;
        case 0x20: set_8bit_register('H', memory.game_rom[cpu.PC+1]); cpu.PC+=2; return 8; break;
        case 0x30: write_into_memory_8bit(extract_adress(cpu.H, cpu.L), memory.game_rom[cpu.PC+1]); cpu.PC+=2; return 12;
        case 0x40: set_8bit_register('B', memory.game_rom[extract_adress(cpu.H, cpu.L)]); cpu.PC+=1; return 8; break;
        case 0x50: set_8bit_register('D', memory.game_rom[extract_adress(cpu.H, cpu.L)]); cpu.PC+=1; return 8; break;
        case 0x60: set_8bit_register('H', memory.game_rom[extract_adress(cpu.H, cpu.L)]); cpu.PC+=1; return 8; break;
        case 0x70: cpu.is_halted = true; cpu.PC++; return 4;
        case 0x80: update_increment_8bit_register(&cpu.A, memory.game_rom[extract_adress(cpu.H, cpu.L)], false, false); cpu.PC++; return 8; break;
        case 0x90: update_decrement_8bit_register(&cpu.A, memory.game_rom[extract_adress(cpu.H, cpu.L)], false, false); cpu.PC++; return 8; break;
        case 0xA0: update_comparator_AND_8bit_register(&cpu.A, memory.game_rom[extract_adress(cpu.H, cpu.L)]); cpu.PC++; return 8; break;
        case 0xB0: update_comparator_OR_8bit_register(&cpu.A, memory.game_rom[extract_adress(cpu.H, cpu.L)]); cpu.PC++; return 8; break;
        case 0xC0: update_increment_8bit_register(&cpu.A, memory.game_rom[cpu.PC+1], false, false); cpu.PC+=2; return 8; break;
        case 0xD0: update_decrement_8bit_register(&cpu.A, memory.game_rom[cpu.PC+1], false, false); cpu.PC+=2; return 4; break;
        case 0xE0: update_comparator_AND_8bit_register(&cpu.A, memory.game_rom[cpu.PC+1]); cpu.PC+=2; return 8; break;
        case 0xF0: update_comparator_OR_8bit_register(&cpu.A, memory.game_rom[cpu.PC+1]); cpu.PC+=2; return 8; break;
        default: break;
    }
}


uint8_t check_operand_row_collum_7(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: break;
        case 0x10: break; // load HL, n16
        case 0x20: break; // load
        case 0x30: break;
        case 0x40: break;
        case 0x50: break;
        case 0x60: break;
        case 0x70: break;//gameboy_ram[extract_adress(cpu.H, cpu.L)] = cpu.A; break;
        case 0x80: break;
        case 0x90: break;
        case 0xA0: break;
        case 0xB0: break;
        case 0xC0: break;
        case 0xD0: break;
        case 0xE0: break;
        case 0xF0: break;
        default: break;
    }
}


uint8_t check_operand_row_collum_8(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: break;
        case 0x10: break; // load HL, n16
        case 0x20: break; // load
        case 0x30: break;
        case 0x40: break;
        case 0x50: break;
        case 0x60: break;
        case 0x70: break;
        case 0x80: break;
        case 0x90: break;
        case 0xA0: break;
        case 0xB0: break;
        case 0xC0: break;
        case 0xD0: break;
        case 0xE0: break;
        case 0xF0: break;
        default: break;
    }
}


uint8_t check_operand_row_collum_9(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: break;
        case 0x10: break; // load HL, n16
        case 0x20: break; // load
        case 0x30: break;
        case 0x40: break;
        case 0x50: break;
        case 0x60: break;
        case 0x70: break;
        case 0x80: break;
        case 0x90: break;
        case 0xA0: break;
        case 0xB0: break;
        case 0xC0: break;
        case 0xD0: break;
        case 0xE0: break;
        case 0xF0: break;
        default: break;
    }
}


uint8_t check_operand_row_collum_A(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: break;
        case 0x10: break; // load HL, n16
        case 0x20: break; // load
        case 0x30: break;
        case 0x40: break;
        case 0x50: break;
        case 0x60: break;
        case 0x70: break;
        case 0x80: break;
        case 0x90: break;
        case 0xA0: break;
        case 0xB0: break;
        case 0xC0: break;
        case 0xD0: break;
        case 0xE0: break;
        case 0xF0: break;
        default: break;
    }
}


uint8_t check_operand_row_collum_B(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: break;
        case 0x10: break; // load HL, n16
        case 0x20: break; // load
        case 0x30: break;
        case 0x40: break;
        case 0x50: break;
        case 0x60: break;
        case 0x70: break;
        case 0x80: break;
        case 0x90: break;
        case 0xA0: break;
        case 0xB0: break;
        case 0xC0: break;
        case 0xD0: break;
        case 0xE0: break;
        case 0xF0: break;
        default: break;
    }
}


uint8_t check_operand_row_collum_C(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: cpu.C++; break;
        case 0x10: cpu.E++; break; // load HL, n16
        case 0x20: cpu.L++; break; // load
        case 0x30: cpu.A++; break;
        case 0x40: break;
        case 0x50: break;
        case 0x60: break;
        case 0x70: break;
        case 0x80: break;
        case 0x90: break;
        case 0xA0: break;
        case 0xB0: break;
        case 0xC0: break;
        case 0xD0: break;
        case 0xE0: break;
        case 0xF0: break;
        default: break;
    }
}

uint8_t check_operand_row_collum_D(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: break;
        case 0x10: break; // load HL, n16
        case 0x20: break; // load
        case 0x30: break;
        case 0x40: break;
        case 0x50: break;
        case 0x60: break;
        case 0x70: break;
        case 0x80: break;
        case 0x90: break;
        case 0xA0: break;
        case 0xB0: break;
        case 0xC0: break;
        case 0xD0: break;
        case 0xE0: break;
        case 0xF0: break;
        default: break;
    }
}


uint8_t check_operand_row_collum_E(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: cond_wait_8bit_var_C = true; break;
        case 0x10: cond_wait_8bit_var_E = true; break; // load HL, n16
        case 0x20: cond_wait_8bit_var_L = true; break; // load
        case 0x30: cond_wait_8bit_var_A = true; break;
        case 0x40: break;
        case 0x50: break;
        case 0x60: break;
        case 0x70: break;
        case 0x80: break;
        case 0x90: break;
        case 0xA0: break;
        case 0xB0: break;
        case 0xC0: break;
        case 0xD0: break;
        case 0xE0: break;
        case 0xF0: break;
        default: break;
    }
}

uint8_t check_operand_row_collum_F(uint8_t opcode) {
    switch (get_opcode_row(opcode)) {
        case 0x00: break;
        case 0x10: break; // load HL, n16
        case 0x20: break; // load
        case 0x30: break;
        case 0x40: break;
        case 0x50: break;
        case 0x60: break;
        case 0x70: break;
        case 0x80: break;
        case 0x90: break;
        case 0xA0: break;
        case 0xB0: break;
        case 0xC0: break;
        case 0xD0: break;
        case 0xE0: break;
        case 0xF0: break;
        default: break;
    }
}

uint8_t check_operand_collumn(uint8_t opcode) {
    switch (get_opcode_collum(opcode)) {
        case 0x00: return check_operand_row_collum_0(opcode); break;
        case 0x01: return check_operand_row_collum_1(opcode); break;
        case 0x02: return check_operand_row_collum_2(opcode); break;
        case 0x03: return check_operand_row_collum_3(opcode); break;
        case 0x04: return check_operand_row_collum_4(opcode); break;
        case 0x05: return check_operand_row_collum_5(opcode); break;
        case 0x06: return check_operand_row_collum_6(opcode); break;
        case 0x07: return check_operand_row_collum_7(opcode); break;
        case 0x08: return check_operand_row_collum_8(opcode); break;
        case 0x09: return check_operand_row_collum_9(opcode); break;
        case 0x0A: return check_operand_row_collum_A(opcode); break;
        case 0x0B: return check_operand_row_collum_B(opcode); break;
        case 0x0C: return check_operand_row_collum_C(opcode); break;
        case 0x0D: return check_operand_row_collum_D(opcode); break;
        case 0x0E: return check_operand_row_collum_E(opcode); break;
        case 0x0F: return check_operand_row_collum_F(opcode); break;
        default: break;
    }
}

int main(void) {
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
    memory.game_rom_lenght = 1;
    memory.MBC_curr_bank = 1;
    start_game("/home/bolota/CLionProjects/untitled/Tetris.gb");
    printf("Gameboy ROM size: %d\n", memory.game_rom_lenght);
    int count = 1;
    printf("%02x\n", memory.game_rom[0x0147]);
    while (1) {
        /*
        bit 0 VBlank → 0x0040   // PPU vai ligar depois
        bit 1 LCD    → 0x0048   // deixa pra depois
        bit 2 Timer  → 0x0050   // dá pra fazer agora
        bit 3 Serial → 0x0058   // ignora por enquanto
        bit 4 Joypad → 0x0060   // joypad vai ligar depois
         */
        if (!cpu.is_halted) {
            //roda normal
            incrementar_ciclos(check_operand_collumn(memory.game_rom[cpu.PC]));
            check_cycle_counter();
            check_if_interrupted();
        }
        else {
            //passa tempo e checa interrupções
            incrementar_ciclos(4);
            check_cycle_counter();
            cpu.is_halted = check_if_interrupted();
        }
    }
    return 0;
}
