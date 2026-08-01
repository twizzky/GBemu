#ifndef CPU_H
#define CPU_H
#define FLAG_Z 7
#define FLAG_N 6
#define FLAG_H 5
#define FLAG_C 4
#define SET_FLAG(cpu,flag) (cpu->F |= (1 << flag))
#define CLEAR_FLAG(cpu,flag) (cpu->F &= ~(1 << flag))
#define GET_FLAG(cpu,flag) ((cpu->F >> flag) & 1)
#define INT_VBLANK 0x40
#define INT_LCD 0x48
#define INT_TIMER 0x50
#define INT_SERIAL 0x58
#define INT_JOYPAD 0x60
#define SET_FLAG_COND(cpu, flag, cond) \
    do { \
        if (cond) SET_FLAG(cpu, flag); \
        else CLEAR_FLAG(cpu, flag); \
    } while (0)

#include <stdint.h>
#include<stdbool.h>
///////////////////////////////////////////////////
//CPU STRUCT:
///////////////////////////////////////////////////
typedef struct MMU MMU;
typedef struct CPU {
    union {  
        struct { uint8_t F, A; }; 
        uint16_t AF;
    };
    union {
        struct { uint8_t C, B; };
        uint16_t BC;
    };
    union {
        struct { uint8_t E, D; };
        uint16_t DE;
    };
    union {
        struct { uint8_t L, H; };
        uint16_t HL;
    };
    uint16_t SP;
    uint16_t PC;
    MMU *mmu;
    uint8_t IME;
    uint16_t internal_div;
    int timer_counter;
    uint8_t halted;
    int cycles;
    int ime_pending;
} CPU;


///////////////////////////////////////////////////
//UTILITY PROTOTYPES:
///////////////////////////////////////////////////
typedef void (*OpCodeHandler)(CPU *cpu);
void initCPU(CPU *cpu, MMU *mmu);
void run(CPU *cpu);
static inline uint8_t cpu_fetch(CPU *cpu);
extern OpCodeHandler opcode_table[256];
/////////////////////////////////////////////////////
//GENERAL PROTOTYPES:
/////////////////////////////////////////////////////
//addition
static inline void cpu_add(CPU *cpu, uint8_t value){
    uint16_t result = cpu->A + value;
    if ((result & 0xFF) == 0) {
        SET_FLAG(cpu, FLAG_Z);
    }
    else{
        CLEAR_FLAG(cpu, FLAG_Z);
    }
    CLEAR_FLAG(cpu, FLAG_N);
    if (((cpu->A & 0x0F)+(value& 0xF)) > 0xF) SET_FLAG(cpu, FLAG_H);
    else CLEAR_FLAG(cpu, FLAG_H);
    if (result > 0xFF) SET_FLAG(cpu, FLAG_C);
    else CLEAR_FLAG(cpu, FLAG_C);
    cpu->A = result & 0xFF;
}
//subtraction
static inline void cpu_sub(CPU *cpu, uint8_t value) {
    uint8_t oldA = cpu->A;


    uint16_t result = (uint16_t)oldA - value;
    cpu->A = (uint8_t)result;   // write A

    // Force flags directly on F (bypass macro issues)
    if (cpu->A == 0)
        cpu->F |= 0x80;
    else
        cpu->F &= ~0x80;

    cpu->F |= 0x40;                    // N = 1

    if ((oldA & 0x0F) < (value & 0x0F))
        cpu->F |= 0x20;
    else
        cpu->F &= ~0x20;

    if (oldA < value)
        cpu->F |= 0x10;                // C = borrow
    else
        cpu->F &= ~0x10;


}
// ADC (add with carry) :
static inline void cpu_adc(CPU *cpu, uint8_t value){
    uint8_t carry = GET_FLAG(cpu, FLAG_C);
    uint16_t result = cpu->A + value + carry;
    if ((result & 0xFF) == 0) SET_FLAG(cpu, FLAG_Z);
    else CLEAR_FLAG(cpu, FLAG_Z);

    CLEAR_FLAG(cpu, FLAG_N);

    if (((cpu->A & 0xF)+ (value & 0xF)+ carry) > 0xF) SET_FLAG(cpu,FLAG_H);
    else CLEAR_FLAG(cpu, FLAG_H);
    
    if(result > 0xFF) SET_FLAG(cpu, FLAG_C);
    else CLEAR_FLAG(cpu, FLAG_C);
    cpu->A = result & 0xFF;
    
}
// SBC (subtract with carry) :
static inline void cpu_sbc(CPU *cpu, uint8_t value) {
    uint8_t carry = GET_FLAG(cpu, FLAG_C);
    uint16_t result = cpu->A - value - carry;

    SET_FLAG(cpu, FLAG_N);                                          // always set
    SET_FLAG_COND(cpu, FLAG_Z, (result & 0xFF) == 0);              // set if result is 0
    SET_FLAG_COND(cpu, FLAG_H, (cpu->A & 0x0F) < (value & 0x0F) + carry); // half borrow
    SET_FLAG_COND(cpu, FLAG_C, cpu->A < (uint16_t)value + carry);  // full borrow

    cpu->A = result & 0xFF;
}
// INC :


static inline void cpu_inc(CPU *cpu, uint8_t *reg) {
    uint8_t before = *reg;
    uint8_t after = before + 1;
    *reg = after;
 
    if (after == 0)
        cpu->F |= 0x80;
    else
        cpu->F &= ~0x80;

    if ((before & 0x0F) == 0x0F)
        cpu->F |= 0x20;
    else
        cpu->F &= ~0x20;

    cpu->F &= ~0x40;

}

// DEC :
static inline void cpu_dec(CPU *cpu, uint8_t *reg) {
    uint8_t before = *reg;
    uint8_t after = before - 1;
    *reg = after;

    SET_FLAG_COND(cpu, FLAG_Z, after == 0);
    SET_FLAG(cpu, FLAG_N);
    SET_FLAG_COND(cpu, FLAG_H, (before & 0x0F) == 0x00);

}
// 16-bit INCREMENT AND DECREMENT :
static inline void cpu_inc_16(CPU *cpu, uint16_t *reg) {
    (*reg)++;
}
static inline void cpu_dec_16(CPU *cpu, uint16_t *reg) {
    (*reg)--;
}
// BITWISE AND OR XOR CP :
//// AND :
static inline void alu_AND(CPU *cpu, uint8_t value) {
    cpu->A &= value;

    // clear all flags first
    CLEAR_FLAG(cpu, FLAG_N);
    SET_FLAG(cpu, FLAG_H);
    CLEAR_FLAG(cpu, FLAG_C);

    SET_FLAG_COND(cpu, FLAG_Z, cpu->A == 0);
}
//// OR with SET FLAG  and SET flag COND :
static inline void alu_or(CPU *cpu, uint8_t value) {
    cpu->A |= value;

    // clear all flags first
    CLEAR_FLAG(cpu, FLAG_N);
    CLEAR_FLAG(cpu, FLAG_H);
    CLEAR_FLAG(cpu, FLAG_C);

    SET_FLAG_COND(cpu, FLAG_Z, cpu->A == 0);
}

//// XOR :
static inline void alu_xor(CPU *cpu, uint8_t value) {
    cpu->A ^= value;

    // clear all flags first
    CLEAR_FLAG(cpu, FLAG_N);
    CLEAR_FLAG(cpu, FLAG_H);
    CLEAR_FLAG(cpu, FLAG_C);

    SET_FLAG_COND(cpu, FLAG_Z, cpu->A == 0);
}
//// CP (compare):
static inline void alu_cp(CPU *cpu, uint8_t value) {
    uint8_t result = cpu->A - value;

    SET_FLAG_COND(cpu, FLAG_Z, cpu->A == value);
    SET_FLAG(cpu, FLAG_N);
    SET_FLAG_COND(cpu, FLAG_H, (cpu->A & 0xF) < (value & 0xF));
    SET_FLAG_COND(cpu, FLAG_C, cpu->A < value);
}

// PUSH POP : 
uint16_t pop16(CPU *cpu);
void push16(CPU *cpu, uint16_t value);

// Jump, CALL, RET operations:
//// Jump:
/////////////////////////
//EXTRA
////////////////////////
// Missing opcode declarations
void opcode_LD_nn_SP(CPU *cpu);
void opcode_LD_SP_HL(CPU *cpu);
void opcode_SP_plus_r8(CPU *cpu);
void opcode_LDH_n_A(CPU *cpu);
void opcode_LDH_A_n(CPU *cpu);
void opcode_DI(CPU *cpu);
void opcode_EI(CPU *cpu);
void opcode_STOP(CPU *cpu);
void opcode_CB_dispatch(CPU *cpu);

///////////////////////////////////////////////////
//PROTOTYPES FOR OPCODE FUNCTIONS:
///////////////////////////////////////////////////
void execute(CPU *cpu, uint8_t opcode);
void opcode_NOP(CPU *cpu);
void opcode_LD_A(CPU *cpu);
void opcode_LD_B(CPU *cpu);
void opcode_LD_C(CPU *cpu);
void opcode_LD_D(CPU *cpu);
void opcode_LD_E(CPU *cpu);
void opcode_LD_H(CPU *cpu);
void opcode_LD_L(CPU *cpu);
//A
void opcode_LD_AA(CPU *cpu);
void opcode_LD_AB(CPU *cpu);
void opcode_LD_AC(CPU *cpu);
void opcode_LD_AD(CPU *cpu);
void opcode_LD_AE(CPU *cpu);
void opcode_LD_AH(CPU *cpu);
void opcode_LD_AL(CPU *cpu);
//B
void opcode_LD_BA(CPU *cpu);
void opcode_LD_BB(CPU *cpu);
void opcode_LD_BC(CPU *cpu);
void opcode_LD_BD(CPU *cpu);
void opcode_LD_BE(CPU *cpu);
void opcode_LD_BH(CPU *cpu);
void opcode_LD_BL(CPU *cpu);
//C
void opcode_LD_CA(CPU *cpu);
void opcode_LD_CB(CPU *cpu);
void opcode_LD_CC(CPU *cpu);
void opcode_LD_CD(CPU *cpu);
void opcode_LD_CE(CPU *cpu);
void opcode_LD_CH(CPU *cpu);
void opcode_LD_CL(CPU *cpu);
//D
void opcode_LD_DA(CPU *cpu);
void opcode_LD_DB(CPU *cpu);
void opcode_LD_DC(CPU *cpu);
void opcode_LD_DD(CPU *cpu);
void opcode_LD_DE(CPU *cpu);
void opcode_LD_DH(CPU *cpu);
void opcode_LD_DL(CPU *cpu);
//E
void opcode_LD_EA(CPU *cpu);
void opcode_LD_EB(CPU *cpu);
void opcode_LD_EC(CPU *cpu);
void opcode_LD_ED(CPU *cpu);
void opcode_LD_EE(CPU *cpu);
void opcode_LD_EH(CPU *cpu);
void opcode_LD_EL(CPU *cpu);
//H
void opcode_LD_HA(CPU *cpu);
void opcode_LD_HB(CPU *cpu);
void opcode_LD_HC(CPU *cpu);
void opcode_LD_HD(CPU *cpu);
void opcode_LD_HE(CPU *cpu);
void opcode_LD_HH(CPU *cpu);
void opcode_LD_HL(CPU *cpu);
//L
void opcode_LD_LA(CPU *cpu);
void opcode_LD_LB(CPU *cpu);
void opcode_LD_LC(CPU *cpu);
void opcode_LD_LD(CPU *cpu);
void opcode_LD_LE(CPU *cpu);
void opcode_LD_LH(CPU *cpu);
void opcode_LD_LL(CPU *cpu);
// HL TO REGISTERS: 
void opcode_LD_HL_A(CPU *cpu);
void opcode_LD_HL_B(CPU *cpu);
void opcode_LD_HL_C(CPU *cpu);
void opcode_LD_HL_D(CPU *cpu);
void opcode_LD_HL_E(CPU *cpu);
void opcode_LD_HL_H(CPU *cpu);
void opcode_LD_HL_L(CPU *cpu);
// REGISTER TO HL: 
void opcode_LD_A_HL(CPU *cpu);
void opcode_LD_B_HL(CPU *cpu);
void opcode_LD_C_HL(CPU *cpu);
void opcode_LD_D_HL(CPU *cpu);
void opcode_LD_E_HL(CPU *cpu);
void opcode_LD_H_HL(CPU *cpu);
void opcode_LD_L_HL(CPU *cpu);
// LOAD FROM C AND NN INTO A : 
void opcode_LD_C_A(CPU *cpu);
void opcode_LD_NN_A(CPU *cpu);
// LOAD FROM A INTO C AND NN : 
void opcode_LD_A_C(CPU *cpu);
void opcode_LD_A_NN(CPU *cpu);
// HL+ HL- INTO A:
void opcode_LD_HL_dec_A(CPU *cpu);
void opcode_LD_HL_inc_A(CPU *cpu);
// A INTO HL+ HL-:
void opcode_LD_A_HL_dec(CPU *cpu);
void opcode_LD_A_HL_inc(CPU *cpu);
void opcode_JP(CPU *cpu);
void opcode_HALT(CPU *cpu);
void opcode_UNKNOWN(CPU *cpu);
// ADD
void opcode_ADD_A_B(CPU *cpu);
void opcode_ADD_A_C(CPU *cpu);
void opcode_ADD_A_D(CPU *cpu);
void opcode_ADD_A_E(CPU *cpu);
void opcode_ADD_A_H(CPU *cpu);
void opcode_ADD_A_L(CPU *cpu);
void opcode_ADD_A_A(CPU *cpu);
void opcode_ADD_A_HL(CPU *cpu);
// SUB
void opcode_SUB_A_B(CPU *cpu);
void opcode_SUB_A_C(CPU *cpu);
void opcode_SUB_A_D(CPU *cpu);
void opcode_SUB_A_E(CPU *cpu);
void opcode_SUB_A_H(CPU *cpu);
void opcode_SUB_A_L(CPU *cpu);
void opcode_SUB_A_A(CPU *cpu);
void opcode_SUB_A_HL(CPU *cpu);
// ADC
void opcode_ADC_A_B(CPU *cpu);
void opcode_ADC_A_C(CPU *cpu);
void opcode_ADC_A_D(CPU *cpu);
void opcode_ADC_A_E(CPU *cpu);
void opcode_ADC_A_H(CPU *cpu);
void opcode_ADC_A_L(CPU *cpu);
void opcode_ADC_A_A(CPU *cpu);
void opcode_ADC_A_HL(CPU *cpu);
// SBC
void opcode_SBC_A_B(CPU*cpu);
void opcode_SBC_A_C(CPU*cpu);
void opcode_SBC_A_D(CPU*cpu);
void opcode_SBC_A_E(CPU*cpu);
void opcode_SBC_A_H(CPU*cpu);
void opcode_SBC_A_L(CPU*cpu);
void opcode_SBC_A_A(CPU*cpu);
void opcode_SBC_A_HL(CPU*cpu);
// INCREMENT AND DECREMENT :
void opcode_INC_B(CPU *cpu);
void opcode_INC_C(CPU *cpu);
void opcode_INC_D(CPU *cpu);
void opcode_INC_E(CPU *cpu);
void opcode_INC_H(CPU *cpu);
void opcode_INC_L(CPU *cpu);
void opcode_INC_A(CPU *cpu);
void opcode_DEC_B(CPU *cpu);
void opcode_DEC_C(CPU *cpu);
void opcode_DEC_D(CPU *cpu);
void opcode_DEC_E(CPU *cpu);
void opcode_DEC_H(CPU *cpu);
void opcode_DEC_L(CPU *cpu);
void opcode_DEC_A(CPU *cpu);
// HL INCREMENT AND DECREMENT :
void opcode_INC_HL(CPU *cpu);
void opcode_DEC_HL(CPU *cpu);
// 16-bit INCREMENT AND DECREMENT :
void opcode_INC_BC(CPU *cpu);
void opcode_INC_DE(CPU *cpu);
void opcode_INC_HL_16(CPU *cpu);
void opcode_INC_SP(CPU *cpu);
void opcode_DEC_BC(CPU *cpu);
void opcode_DEC_DE(CPU *cpu);
void opcode_DEC_HL_16(CPU *cpu);
void opcode_DEC_SP(CPU *cpu);
// BITWISE AND OR XOR CP :
void opcode_AND_A(CPU *cpu);
void opcode_AND_B(CPU *cpu);
void opcode_AND_C(CPU *cpu);
void opcode_AND_D(CPU *cpu);
void opcode_AND_E(CPU *cpu);
void opcode_AND_H(CPU *cpu);
void opcode_AND_L(CPU *cpu);
void opcode_AND_HL(CPU *cpu);
void opcode_OR_A(CPU *cpu);
void opcode_OR_B(CPU *cpu);
void opcode_OR_D(CPU *cpu);
void opcode_OR_E(CPU *cpu);
void opcode_OR_H(CPU *cpu);
void opcode_OR_L(CPU *cpu);
void opcode_OR_HL(CPU *cpu);
void opcode_XOR_A(CPU *cpu);
void opcode_XOR_B(CPU *cpu);
void opcode_XOR_C(CPU *cpu);
void opcode_XOR_D(CPU *cpu);
void opcode_XOR_E(CPU *cpu);
void opcode_XOR_H(CPU *cpu);
void opcode_XOR_L(CPU *cpu);
void opcode_XOR_HL(CPU *cpu);
void opcode_CP_A(CPU *cpu);
void opcode_CP_B(CPU *cpu);
void opcode_CP_C(CPU *cpu);
void opcode_CP_D(CPU *cpu);
void opcode_CP_E(CPU *cpu);
void opcode_CP_H(CPU *cpu);
void opcode_CP_L(CPU *cpu);
void opcode_CP_HL(CPU *cpu);
// JP, RET, RETI, CALL:
void opcode_JP(CPU *cpu);
void opcode_JP_NZ(CPU *cpu);
void opcode_JP_Z(CPU *cpu);
void opcode_JP_NC(CPU *cpu);
void opcode_JP_C(CPU *cpu);
void opcode_JP_HL(CPU *cpu);
void opcode_CALL(CPU *cpu);
void opcode_RET(CPU *cpu);
void opcode_RETI(CPU *cpu);
//Jr instructions:
void opcode_JR(CPU *cpu);
void opcode_JR_NZ(CPU *cpu);
void opcode_JR_Z(CPU *cpu);
void opcode_JR_NC(CPU *cpu);
void opcode_JR_C(CPU *cpu);
//Call and Ret instructions:
void opcode_CALL(CPU *cpu);
void opcode_CALL_NZ(CPU *cpu);
void opcode_CALL_Z(CPU *cpu);
void opcode_CALL_NC(CPU *cpu);
void opcode_CALL_C(CPU *cpu);
// Ret instructions:
void opcode_RET(CPU *cpu);
void opcode_RET_NZ(CPU *cpu);
void opcode_RET_Z(CPU *cpu);
void opcode_RET_NC(CPU *cpu);
void opcode_RET_C(CPU *cpu);
void opcode_RETI(CPU *cpu);
// RST instructions:
void opcode_RST_00(CPU *cpu);
void opcode_RST_08(CPU *cpu);
void opcode_RST_10(CPU *cpu);
void opcode_RST_18(CPU *cpu);
void opcode_RST_20(CPU *cpu);
void opcode_RST_28(CPU *cpu);
void opcode_RST_30(CPU *cpu);
void opcode_RST_38(CPU *cpu);

// ALU
void opcode_SUB_N(CPU *cpu);
void opcode_ADD_N(CPU *cpu);
void opcode_ADC_N(CPU *cpu);
void opcode_SBC_N(CPU *cpu);
void opcode_CP_N(CPU *cpu);
void opcode_SUB_B(CPU *cpu);
void opcode_OR_C(CPU *cpu);
void opcode_OR_N(CPU *cpu);
void opcode_XOR_N(CPU *cpu);
void opcode_AND_N(CPU *cpu);
// Stack
void opcode_POP_AF(CPU *cpu);
void opcode_PUSH_DE(CPU *cpu);
void opcode_POP_BC(CPU *cpu);
void opcode_POP_HL(CPU *cpu);
void opcode_PUSH_AF(CPU *cpu);
void opcode_PUSH_BC(CPU *cpu);
void opcode_PUSH_HL(CPU *cpu);
void opcode_POP_DE(CPU *cpu);
// Misc
void opcode_DI(CPU *cpu);
void opcode_STOP(CPU *cpu);

// Loads
void opcode_LD_SP_NN(CPU *cpu);
void opcode_LD_DE_NN(CPU *cpu);
void opcode_LD_BC_A(CPU *cpu);
void opcode_LD_A_BC(CPU *cpu);
void opcode_LD_HL_NN(CPU *cpu);
void opcode_LD_HL_N(CPU *cpu);
void opcode_LD_BC_NN(CPU *cpu);
void opcode_LD_NN_A(CPU *cpu);
void opcode_LDH_N_A(CPU *cpu);
void opcode_LD_A_N(CPU *cpu);
void opcode_LD_A_L(CPU *cpu);
void opcode_LD_A_H(CPU *cpu);
void opcode_LD_A_B(CPU *cpu);
void opcode_LD_A_NN(CPU *cpu);
void opcode_LD_A_HLI(CPU *cpu);
void opcode_LD_DE_A(CPU *cpu);
void opcode_LD_A_DE(CPU *cpu);
void opcode_ADD_SP_plus_r8(CPU *cpu);

//opcodes RLCA, RRCA, RLA,RRA:
void opcode_RLCA(CPU *cpu);
void opcode_RRCA(CPU *cpu);
void opcode_RLA(CPU *cpu);
void opcode_RRA(CPU *cpu);

// CB OPCODES:

// CCF and CPL:
void opcode_CPL(CPU *cpu);
void opcode_CCF(CPU *cpu);
void opcode_DAA(CPU *cpu);
// ALL HL opcodes including ADD
void opcode_ADD_HL_BC(CPU *cpu);
void opcode_ADD_HL_DE(CPU *cpu);
void opcode_ADD_HL_HL(CPU *cpu);
void opcode_ADD_HL_SP(CPU *cpu);
//dump : 
void dump_vram_text(MMU *mmu);
///////////////////////////////////////////////////
#endif
