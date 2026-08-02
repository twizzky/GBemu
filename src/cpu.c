#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "cpu.h"
#include "memory.h" 
#include "ppu.h"
///////////////////////////////////////////////////
// INITIALIZATION HERE
///////////////////////////////////////////////////
FILE *logfile;
void init_logging() {
    logfile = fopen("cpu.log", "w");
}
static void init_cb_cycles(void);
void initCPU(CPU *cpu, MMU *mmu) {
    cpu->AF = 0x01B0;
    cpu->BC = 0x0013;
    cpu->DE = 0x00D8;
    cpu->HL = 0x014D;
    cpu->PC = 0x0100;
    cpu->SP = 0xFFFE;
    cpu->mmu = mmu;
    mmu->cpu = cpu;
    cpu->timer_counter = 0;
    cpu->internal_div = 0;
    cpu->halted = 0;
    cpu->IME = 0;
    cpu->cycles = 0;
    cpu->ime_pending = 0;
    init_cb_cycles();
}
// PUSH POP : 
uint16_t pop16(CPU *cpu) {
    uint16_t addr = cpu->SP;
    uint8_t lo = mmu_read8(cpu->mmu, cpu->SP++);
    uint8_t hi = mmu_read8(cpu->mmu, cpu->SP++);
    uint16_t value = ((uint16_t)hi << 8) | lo;

    return value;
}

void push16(CPU *cpu, uint16_t value) {

    mmu_write8(cpu->mmu, --cpu->SP, (value >> 8) & 0xFF); // high
    mmu_write8(cpu->mmu, --cpu->SP, value & 0xFF);        // low
}

///////////////////////////////////////////////////
// HELPERS (JP,RETI,CALL,RET):
///////////////////////////////////////////////////
void opcode_JP(CPU *cpu){
    uint16_t addr = cpu_fetch(cpu);
    addr |= (cpu_fetch(cpu) << 8);
    cpu->PC = addr;
}
void opcode_JP_NZ(CPU *cpu){
    uint16_t addr = cpu_fetch(cpu);
    addr |= (cpu_fetch(cpu)<<8);
    if(!GET_FLAG(cpu, FLAG_Z)) cpu->PC = addr;
}
void opcode_JP_Z(CPU *cpu){
    uint16_t addr = cpu_fetch(cpu);
    addr |= (cpu_fetch(cpu)<< 8);
    if (GET_FLAG(cpu, FLAG_Z)) cpu-> PC = addr;
}
void opcode_JP_NC(CPU *cpu) {
    uint16_t addr = cpu_fetch(cpu);
    addr |= (cpu_fetch(cpu) << 8);
    if (!GET_FLAG(cpu, FLAG_C)) cpu->PC = addr;
}
void opcode_JP_C(CPU *cpu){
    uint16_t addr = cpu_fetch(cpu);
    addr |= (cpu_fetch(cpu) << 8);
    if(GET_FLAG(cpu, FLAG_C)) cpu->PC = addr;
}
void opcode_JP_HL(CPU *cpu){
    cpu->PC = cpu->HL;
}
// Jr : 
void opcode_JR(CPU *cpu) {
    int8_t offset = (int8_t)cpu_fetch(cpu);
    cpu->PC += offset;
}
void opcode_JR_NZ(CPU *cpu){
    uint16_t pc_before = cpu->PC;
    int8_t offset = (int8_t) cpu_fetch(cpu);
    bool take_jump = !GET_FLAG(cpu, FLAG_Z);


    if (take_jump) {
        cpu->PC += offset;
    } else {
    }
}

void opcode_JR_Z(CPU *cpu){
    int8_t offset = (int8_t) cpu_fetch(cpu);
    if (GET_FLAG(cpu, FLAG_Z)) {
        cpu->PC += offset;
    }
}
void opcode_JR_NC(CPU *cpu) {
    int8_t offset = (int8_t) cpu_fetch(cpu);
    bool take_jump = !GET_FLAG(cpu, FLAG_C);


    if (take_jump) {
        cpu->PC += offset;
    } else {
    }
}
void opcode_JR_C(CPU *cpu){
    int8_t offset = (int8_t) cpu_fetch(cpu);
    if (GET_FLAG(cpu, FLAG_C)) {
        cpu->PC += offset;
    }
}
// Call and Ret instructions :
void opcode_CALL(CPU *cpu) {
    uint16_t addr = cpu_fetch(cpu);
    addr |= (cpu_fetch(cpu) << 8);
    cpu->SP -= 2;
    mmu_write16(cpu->mmu, cpu->SP, cpu->PC);
    cpu->PC = addr;
}

void opcode_CALL_NZ(CPU *cpu) {
    uint16_t addr = cpu_fetch(cpu);
    addr |= (cpu_fetch(cpu) << 8);
    if (!GET_FLAG(cpu, FLAG_Z)) {
        push16(cpu, cpu->PC);
        cpu->PC = addr;
    }
}

void opcode_CALL_Z(CPU *cpu) {
    uint16_t addr = cpu_fetch(cpu);
    addr |= (cpu_fetch(cpu) << 8);
    if (GET_FLAG(cpu, FLAG_Z)) {
        push16(cpu, cpu->PC);
        cpu->PC = addr;
    }
}

void opcode_CALL_NC(CPU *cpu) {
    uint16_t addr = cpu_fetch(cpu);
    addr |= (cpu_fetch(cpu) << 8);
    if (!GET_FLAG(cpu, FLAG_C)) {
        push16(cpu, cpu->PC);
        cpu->PC = addr;
    }
}

void opcode_CALL_C(CPU *cpu) {
    uint16_t addr = cpu_fetch(cpu);
    addr |= (cpu_fetch(cpu) << 8);
    if (GET_FLAG(cpu, FLAG_C)) {
        push16(cpu, cpu->PC);
        cpu->PC = addr;
    }
}
void opcode_RET(CPU *cpu) {
    cpu->PC = pop16(cpu);
}

void opcode_RET_NZ(CPU *cpu) {
    if (!GET_FLAG(cpu, FLAG_Z)) {
        cpu->PC = pop16(cpu);
    }
}

void opcode_RET_Z(CPU *cpu) {
    if (GET_FLAG(cpu, FLAG_Z)) {
        cpu->PC = pop16(cpu);
    }
}

void opcode_RET_NC(CPU *cpu) {
    if (!GET_FLAG(cpu, FLAG_C)) {
        cpu->PC = pop16(cpu);
    }
}

void opcode_RET_C(CPU *cpu) {
    if (GET_FLAG(cpu, FLAG_C)) {
        cpu->PC = pop16(cpu);
    }
}
void opcode_RETI(CPU *cpu) {
    cpu->PC = pop16(cpu);
    cpu->IME = 1;
}

// RST : 
void opcode_RST_00(CPU *cpu) { push16(cpu, cpu->PC); cpu->PC = 0x00; }
void opcode_RST_08(CPU *cpu) { push16(cpu, cpu->PC); cpu->PC = 0x08; }
void opcode_RST_10(CPU *cpu) { push16(cpu, cpu->PC); cpu->PC = 0x10; }
void opcode_RST_18(CPU *cpu) { push16(cpu, cpu->PC); cpu->PC = 0x18; }
void opcode_RST_20(CPU *cpu) { push16(cpu, cpu->PC); cpu->PC = 0x20; }
void opcode_RST_28(CPU *cpu) { push16(cpu, cpu->PC); cpu->PC = 0x28; }
void opcode_RST_30(CPU *cpu) { push16(cpu, cpu->PC); cpu->PC = 0x30; }
void opcode_RST_38(CPU *cpu) { push16(cpu, cpu->PC); cpu->PC = 0x38; }

////////////////////////////////////////////////////
// INTERRUPT HANDLING FUNCTIONS :
///////////////////////////////////////////////////
void check_interrupts(CPU *cpu){
    uint8_t IE = mmu_read8(cpu->mmu, 0xFFFF);
    uint8_t IF = mmu_read8(cpu->mmu, 0xFF0F);
    if (!cpu->IME || !(IE & IF & 0x1F)) return;
    for (int i = 0; i < 5; i++){
        if((IE & (1 << i)) && (IF & (1 << i))){
            cpu->IME = 0;
            mmu_write8(cpu->mmu, 0xFF0F, IF & ~(1 << i));
            if (i == 2) cpu->timer_counter = 0;
            push16(cpu, cpu->PC);
            switch (i){
                case 0: cpu->PC = INT_VBLANK; break;
                case 1: cpu->PC = INT_LCD; break;
                case 2: cpu->PC = INT_TIMER; break;
                case 3: cpu->PC = INT_SERIAL; break;
                case 4: cpu->PC = INT_JOYPAD; break;
            }
            break;
        }
    }
}
///////////////////////////////////////////////////
// TIMER FUNCTIONS : 
///////////////////////////////////////////////////
void update_timers(CPU *cpu, int cycles){
    cpu->internal_div += cycles * 4; // DIV counts dots; 1 machine cycle = 4 dots
    uint8_t tac = mmu_read8(cpu->mmu, 0xFF07);
    if(!(tac &0x04)) return; // Timer is disabled
    static const int timer_freqs[4] = {256, 4, 16, 64};
    int timer_freq = timer_freqs[tac & 0x03];
    cpu->timer_counter += cycles;
    while(cpu->timer_counter >= timer_freq){
        cpu->timer_counter -= timer_freq;
        uint8_t tima = mmu_read8(cpu->mmu, 0xFF05);
        if (tima == 0xFF)
        {
                        mmu_write8(cpu->mmu,0xFF05, mmu_read8(cpu->mmu ,0xFF06));
            uint8_t IF = mmu_read8(cpu->mmu, 0xFF0F);
            mmu_write8(cpu->mmu, 0xFF0F,IF | 0x04);

        }
        else
        {
            mmu_write8(cpu->mmu, 0xFF05, tima + 1);
        }
        
    }
}

///////////////////////////////////////////////////
// OPCODE FUNCTIONS ARE HERE : 
///////////////////////////////////////////////////
void opcode_NOP(CPU *cpu){
    // DO NOTHING
}
// 8 BIT LOADING INSTRUCTIONS :
void opcode_LD_A(CPU *cpu){
    cpu->A = cpu_fetch(cpu);
}

void opcode_LD_B(CPU *cpu){
    cpu->B = cpu_fetch(cpu);
}

void opcode_LD_C(CPU *cpu){
    cpu->C = cpu_fetch(cpu);
}

void opcode_LD_D(CPU *cpu){
    cpu->D = cpu_fetch(cpu);
}

void opcode_LD_E(CPU *cpu){
    cpu->E = cpu_fetch(cpu);
}

void opcode_LD_H(CPU *cpu){
    cpu->H = cpu_fetch(cpu);
}

void opcode_LD_L(CPU *cpu){
    cpu->L = cpu_fetch(cpu);
}

//REGISTER TO REGISTER : 
void opcode_LD_AA(CPU *cpu){cpu->A = cpu->A;}

void opcode_LD_AB(CPU *cpu){cpu->A = cpu->B;}

void opcode_LD_AC(CPU *cpu){cpu->A = cpu->C;}

void opcode_LD_AD(CPU *cpu){cpu->A = cpu->D;}

void opcode_LD_AE(CPU *cpu){cpu->A = cpu->E;}

void opcode_LD_AH(CPU *cpu){cpu->A = cpu->H;}

void opcode_LD_AL(CPU *cpu){cpu->A = cpu->L;}
//B
void opcode_LD_BA(CPU *cpu){cpu->B = cpu->A;}

void opcode_LD_BB(CPU *cpu){cpu->B = cpu->B;}

void opcode_LD_BC(CPU *cpu){cpu->B = cpu->C;}

void opcode_LD_BD(CPU *cpu){ cpu->B = cpu->D; }

void opcode_LD_BE(CPU *cpu){ cpu->B = cpu->E; }

void opcode_LD_BH(CPU *cpu){ cpu->B = cpu->H; }

void opcode_LD_BL(CPU *cpu){ cpu->B = cpu->L; }
//C:

void opcode_LD_CA(CPU *cpu){ cpu->C = cpu->A; }

void opcode_LD_CB(CPU *cpu){ cpu->C = cpu->B; }

void opcode_LD_CC(CPU *cpu){ cpu->C = cpu->C; }

void opcode_LD_CD(CPU *cpu){ cpu->C = cpu->D; }

void opcode_LD_CE(CPU *cpu){ cpu->C = cpu->E; }

void opcode_LD_CH(CPU *cpu){ cpu->C = cpu->H; }

void opcode_LD_CL(CPU *cpu){ cpu->C = cpu->L; }
//D:

void opcode_LD_DA(CPU *cpu){ cpu->D = cpu->A; }

void opcode_LD_DB(CPU *cpu){ cpu->D = cpu->B; }

void opcode_LD_DC(CPU *cpu){ cpu->D = cpu->C; }

void opcode_LD_DD(CPU *cpu){ cpu->D = cpu->D; }

void opcode_LD_DE(CPU *cpu){ cpu->D = cpu->E; }

void opcode_LD_DH(CPU *cpu){ cpu->D = cpu->H; }

void opcode_LD_DL(CPU *cpu){ cpu->D = cpu->L; }
//E:

void opcode_LD_EA(CPU *cpu){ cpu->E = cpu->A; }

void opcode_LD_EB(CPU *cpu){ cpu->E = cpu->B; }

void opcode_LD_EC(CPU *cpu){ cpu->E = cpu->C; }

void opcode_LD_ED(CPU *cpu){ cpu->E = cpu->D; }

void opcode_LD_EE(CPU *cpu){ cpu->E = cpu->E; }

void opcode_LD_EH(CPU *cpu){ cpu->E = cpu->H; }

void opcode_LD_EL(CPU *cpu){ cpu->E = cpu->L; }
// H:

void opcode_LD_HA(CPU *cpu){ cpu->H = cpu->A; }

void opcode_LD_HB(CPU *cpu){ cpu->H = cpu->B; }

void opcode_LD_HC(CPU *cpu){ cpu->H = cpu->C; }

void opcode_LD_HD(CPU *cpu){ cpu->H = cpu->D; }

void opcode_LD_HE(CPU *cpu){ cpu->H = cpu->E; }

void opcode_LD_HH(CPU *cpu){ cpu->H = cpu->H; }

void opcode_LD_HL(CPU *cpu){ cpu->H = cpu->L; }

// L :
void opcode_LD_LA(CPU *cpu){ cpu->L = cpu->A; }

void opcode_LD_LB(CPU *cpu){ cpu->L = cpu->B; }

void opcode_LD_LC(CPU *cpu){ cpu->L = cpu->C; }

void opcode_LD_LD(CPU *cpu){ cpu->L = cpu->D; }

void opcode_LD_LE(CPU *cpu){ cpu->L = cpu->E; }

void opcode_LD_LH(CPU *cpu){ cpu->L = cpu->H; }

void opcode_LD_LL(CPU *cpu){ cpu->L = cpu->L; }
// LOAD FROM NN TO REGISTERS :
void opcode_LD_NN_A(CPU *cpu) {
    uint16_t addr = cpu_fetch(cpu);
    addr |= (cpu_fetch(cpu) << 8);
    mmu_write8(cpu->mmu, addr, cpu->A);
}
void opcode_LD_A_NN(CPU *cpu) {
    uint16_t addr = cpu_fetch(cpu);
    addr |= (cpu_fetch(cpu) << 8);
    cpu->A = mmu_read8(cpu->mmu, addr);
}
// HL TO REGISTERS: 
void opcode_LD_HL_A(CPU *cpu) {
    mmu_write8(cpu->mmu, cpu->HL, cpu->A);
}
void opcode_LD_HL_B(CPU *cpu){ mmu_write8(cpu->mmu, cpu->HL, cpu->B);}
void opcode_LD_HL_C(CPU *cpu){ mmu_write8(cpu->mmu, cpu->HL, cpu->C);}
void opcode_LD_HL_D(CPU *cpu){ mmu_write8(cpu->mmu, cpu->HL, cpu->D);}
void opcode_LD_HL_E(CPU *cpu){ mmu_write8(cpu->mmu, cpu->HL, cpu->E);}
void opcode_LD_HL_H(CPU *cpu){ mmu_write8(cpu->mmu, cpu->HL, cpu->H);}
void opcode_LD_HL_L(CPU *cpu){  mmu_write8(cpu->mmu, cpu->HL, cpu->L);}
// REGISTERS TO HL:
void opcode_LD_A_HL(CPU *cpu){ 
    cpu->A = mmu_read8(cpu->mmu, cpu->HL);
}
void opcode_LD_B_HL(CPU *cpu){ 
    cpu->B = mmu_read8(cpu->mmu, cpu->HL);
}
void opcode_LD_C_HL(CPU *cpu){ 
    cpu->C = mmu_read8(cpu->mmu, cpu->HL);
}
void opcode_LD_D_HL(CPU *cpu){ 
    cpu->D = mmu_read8(cpu->mmu, cpu->HL);
}
void opcode_LD_E_HL(CPU *cpu){ 
    cpu->E = mmu_read8(cpu->mmu, cpu->HL);
}
void opcode_LD_H_HL(CPU *cpu){ 
    cpu->H = mmu_read8(cpu->mmu, cpu->HL);
}
void opcode_LD_L_HL(CPU *cpu){ 
    cpu->L = mmu_read8(cpu->mmu, cpu->HL);
}
// C + ADDY/NNto A:
void opcode_LD_A_C(CPU *cpu){cpu->A = mmu_read8(cpu->mmu, 0xFF00 + cpu->C);}
// A to C+ADDY/NN
void opcode_LD_C_A(CPU *cpu){mmu_write8(cpu->mmu, 0xFF00 + cpu->C, cpu->A);}
//HALT: 
void opcode_HALT(CPU *cpu) {
    cpu->halted = 1;
}
// H+/H- VARIANTS:
void opcode_LD_A_HL_inc(CPU *cpu) {
    cpu->A = mmu_read8(cpu->mmu, cpu->HL);
    cpu->HL++;
}
void opcode_LD_A_HL_dec(CPU *cpu) {
    cpu->A = mmu_read8(cpu->mmu, cpu->HL);
    cpu->HL--;
}
void opcode_LD_HL_inc_A(CPU *cpu) {
    mmu_write8(cpu->mmu, cpu->HL, cpu->A);
    cpu->HL++;
}
void opcode_LD_HL_dec_A(CPU *cpu) {
    mmu_write8(cpu->mmu, cpu->HL, cpu->A);
    cpu->HL--;
}
//ADDITION : 
void opcode_ADD_A_B(CPU *cpu){cpu_add(cpu,cpu->B);}
void opcode_ADD_A_C(CPU *cpu){cpu_add(cpu,cpu->C);}
void opcode_ADD_A_D(CPU *cpu){cpu_add(cpu,cpu->D);}
void opcode_ADD_A_E(CPU *cpu){cpu_add(cpu,cpu->E);}
void opcode_ADD_A_H(CPU *cpu){cpu_add(cpu,cpu->H);}
void opcode_ADD_A_L(CPU *cpu){cpu_add(cpu,cpu->L);}
void opcode_ADD_A_A(CPU *cpu){cpu_add(cpu,cpu->A);}

// HL ADDITION : 
void opcode_ADD_A_HL(CPU *cpu){
    uint8_t value = mmu_read8(cpu->mmu, cpu->HL);
    cpu_add(cpu,value);
    }
//SUBTRACTION :
void opcode_SUB_A_B(CPU *cpu){ cpu_sub(cpu,cpu->B);}
void opcode_SUB_A_C(CPU *cpu){ cpu_sub(cpu,cpu->C);}
void opcode_SUB_A_D(CPU *cpu){ cpu_sub(cpu,cpu->D);}
void opcode_SUB_A_E(CPU *cpu){ cpu_sub(cpu,cpu->E);}
void opcode_SUB_A_H(CPU *cpu){ cpu_sub(cpu,cpu->H);}
void opcode_SUB_A_L(CPU *cpu){ cpu_sub(cpu,cpu->L);}
void opcode_SUB_A_A(CPU *cpu){ cpu_sub(cpu,cpu->A);}
// N OPS:
void opcode_SUB_N(CPU *cpu){
    uint8_t value = cpu_fetch(cpu);
    cpu_sub(cpu,value);
}
void opcode_ADD_N(CPU *cpu){
    uint8_t value = cpu_fetch(cpu);
    cpu_add(cpu,value);
}
void opcode_ADC_N(CPU *cpu){
    uint8_t value = cpu_fetch(cpu);
    cpu_adc(cpu,value);
}
void opcode_SBC_N(CPU *cpu){
    uint8_t value = cpu_fetch(cpu);
    cpu_sbc(cpu,value);
}
// HL SUBTRACTION : 
void opcode_SUB_A_HL(CPU *cpu){ 
    uint8_t value = mmu_read8(cpu->mmu, cpu->HL);
    cpu_sub(cpu,value);
    }
//ADC :
void opcode_ADC_A_B(CPU *cpu){cpu_adc(cpu,cpu->B);}
void opcode_ADC_A_C(CPU *cpu){cpu_adc(cpu,cpu->C);}
void opcode_ADC_A_D(CPU *cpu){cpu_adc(cpu,cpu->D);}
void opcode_ADC_A_E(CPU *cpu){cpu_adc(cpu,cpu->E);}
void opcode_ADC_A_H(CPU *cpu){cpu_adc(cpu,cpu->H);}
void opcode_ADC_A_L(CPU *cpu){cpu_adc(cpu,cpu->L);}
void opcode_ADC_A_A(CPU *cpu){cpu_adc(cpu,cpu->A);}

// HL ADC :
void opcode_ADC_A_HL(CPU *cpu){
    uint8_t value = mmu_read8(cpu->mmu, cpu->HL);
    cpu_adc(cpu,value);
}
//SBC :
void opcode_SBC_A_B(CPU*cpu){cpu_sbc(cpu,cpu->B);};
void opcode_SBC_A_C(CPU*cpu){cpu_sbc(cpu,cpu->C);};
void opcode_SBC_A_D(CPU*cpu){cpu_sbc(cpu,cpu->D);};
void opcode_SBC_A_E(CPU*cpu){cpu_sbc(cpu,cpu->E);};
void opcode_SBC_A_H(CPU*cpu){cpu_sbc(cpu,cpu->H);};
void opcode_SBC_A_L(CPU*cpu){cpu_sbc(cpu,cpu->L);};
void opcode_SBC_A_A(CPU*cpu){cpu_sbc(cpu,cpu->A);};
// HL SBC :
void opcode_SBC_A_HL(CPU *cpu) {
    uint8_t val = mmu_read8(cpu->mmu, (cpu->H << 8) | cpu->L);
    uint8_t carry = GET_FLAG(cpu, FLAG_C) ? 1 : 0;
    uint16_t full = cpu->A - val - carry;
    SET_FLAG_COND(cpu, FLAG_Z, (full & 0xFF) == 0);
    SET_FLAG(cpu, FLAG_N);
    SET_FLAG_COND(cpu, FLAG_H, ((cpu->A & 0x0F) - (val & 0x0F) - carry) & 0x10);
    SET_FLAG_COND(cpu, FLAG_C, full > 0xFF);
    cpu->A = (uint8_t)full;
}

// INCREMENT AND DECREMENT :
// INC : 
void opcode_INC_B(CPU *cpu){ cpu_inc(cpu, &cpu->B); }
void opcode_INC_C(CPU *cpu){ cpu_inc(cpu, &cpu->C); }
void opcode_INC_D(CPU *cpu){ cpu_inc(cpu, &cpu->D); }
void opcode_INC_E(CPU *cpu){
    cpu_inc(cpu, &cpu->E);
}
void opcode_INC_H(CPU *cpu){ cpu_inc(cpu, &cpu->H); }
void opcode_INC_L(CPU *cpu) {
    cpu_inc(cpu, &cpu->L);      
}

void opcode_INC_A(CPU *cpu){ cpu_inc(cpu, &cpu->A); }
// DEC :
void opcode_DEC_B(CPU *cpu){ cpu_dec(cpu, &cpu->B); }
void opcode_DEC_C(CPU *cpu){ cpu_dec(cpu, &cpu->C); }
void opcode_DEC_D(CPU *cpu){ cpu_dec(cpu, &cpu->D); }
void opcode_DEC_E(CPU *cpu){ cpu_dec(cpu, &cpu->E); }
void opcode_DEC_H(CPU *cpu){ cpu_dec(cpu, &cpu->H); }
void opcode_DEC_L(CPU *cpu){ cpu_dec(cpu, &cpu->L); }
void opcode_DEC_A(CPU *cpu){ cpu_dec(cpu, &cpu->A); }
// HL INCREMENT AND DECREMENT :
// INC HL :
void opcode_INC_HL(CPU *cpu){
    uint8_t value = mmu_read8(cpu->mmu, cpu->HL);
    cpu_inc(cpu,&value);
    mmu_write8(cpu->mmu, cpu->HL, value);
}
// DEC HL :
void opcode_DEC_HL(CPU *cpu){
    uint8_t value = mmu_read8(cpu->mmu, cpu->HL);
    cpu_dec(cpu,&value);
    mmu_write8(cpu->mmu, cpu->HL, value);
}
// 16-bit INCREMENT AND DECREMENT :
// INC 16-bit :
void opcode_INC_BC(CPU *cpu){ cpu_inc_16(cpu, &cpu->BC); }
void opcode_INC_DE(CPU *cpu){ cpu_inc_16(cpu, &cpu->DE); }
void opcode_INC_HL_16(CPU *cpu){ cpu_inc_16(cpu, &cpu->HL); }
void opcode_INC_SP(CPU *cpu){ cpu_inc_16(cpu, &cpu->SP); }
// DEC 16-bit :
void opcode_DEC_BC(CPU *cpu){ cpu_dec_16(cpu, &cpu->BC); }
void opcode_DEC_DE(CPU *cpu){ cpu_dec_16(cpu, &cpu->DE); }
void opcode_DEC_HL_16(CPU *cpu){ cpu_dec_16(cpu, &cpu->HL); }
void opcode_DEC_SP(CPU *cpu){ cpu_dec_16(cpu, &cpu->SP); }
// AND OR XOR CP :
//// AND :
void opcode_AND_A(CPU *cpu) { alu_AND(cpu, cpu->A); }
void opcode_AND_B(CPU *cpu) { alu_AND(cpu, cpu->B); }
void opcode_AND_C(CPU *cpu) { alu_AND(cpu, cpu->C); }
void opcode_AND_D(CPU *cpu) { alu_AND(cpu, cpu->D); }
void opcode_AND_E(CPU *cpu) { alu_AND(cpu, cpu->E); }
void opcode_AND_H(CPU *cpu) { alu_AND(cpu, cpu->H); }
void opcode_AND_L(CPU *cpu) { alu_AND(cpu, cpu->L); }
void opcode_AND_HL(CPU *cpu) { alu_AND(cpu, mmu_read8(cpu->mmu, cpu->HL)); }
//// OR :
void opcode_OR_A(CPU *cpu) { alu_or(cpu, cpu->A); }
void opcode_OR_B(CPU *cpu) { alu_or(cpu, cpu->B); }
void opcode_OR_C(CPU *cpu) { alu_or(cpu, cpu->C); }
void opcode_OR_D(CPU *cpu) { alu_or(cpu, cpu->D); }
void opcode_OR_E(CPU *cpu) { alu_or(cpu, cpu->E); }
void opcode_OR_H(CPU *cpu) { alu_or(cpu, cpu->H); }
void opcode_OR_L(CPU *cpu) { alu_or(cpu, cpu->L); }
void opcode_OR_HL(CPU *cpu) { alu_or(cpu, mmu_read8(cpu->mmu, cpu->HL)); }
//// XOR :
void opcode_XOR_A(CPU *cpu) { alu_xor(cpu, cpu->A); }
void opcode_XOR_B(CPU *cpu) { alu_xor(cpu, cpu->B); }
void opcode_XOR_C(CPU *cpu) { alu_xor(cpu, cpu->C); }
void opcode_XOR_D(CPU *cpu) { alu_xor(cpu, cpu->D); }
void opcode_XOR_E(CPU *cpu) { alu_xor(cpu, cpu->E); }
void opcode_XOR_H(CPU *cpu) { alu_xor(cpu, cpu->H); }
void opcode_XOR_L(CPU *cpu) { alu_xor(cpu, cpu->L); }
void opcode_XOR_HL(CPU *cpu) { alu_xor(cpu, mmu_read8(cpu->mmu, cpu->HL)); }
/// CP (compare):

void opcode_CP_A(CPU *cpu) { alu_cp(cpu, cpu->A); }
void opcode_CP_B(CPU *cpu) { alu_cp(cpu, cpu->B); }
void opcode_CP_C(CPU *cpu) { alu_cp(cpu, cpu->C); }
void opcode_CP_D(CPU *cpu) { alu_cp(cpu, cpu->D); }
void opcode_CP_E(CPU *cpu) { alu_cp(cpu, cpu->E); }
void opcode_CP_H(CPU *cpu) { alu_cp(cpu, cpu->H); }
void opcode_CP_L(CPU *cpu) { alu_cp(cpu, cpu->L); }
void opcode_CP_HL(CPU *cpu) { alu_cp(cpu, mmu_read8(cpu->mmu, cpu->HL)); }
// AND XOR CP OR n :
void opcode_AND_N(CPU *cpu) {
    uint8_t value = cpu_fetch(cpu);
    alu_AND(cpu, value);
}
void opcode_OR_N(CPU *cpu) {
    uint8_t value = cpu_fetch(cpu);
    alu_or(cpu, value);
}
void opcode_XOR_N(CPU *cpu) {
    uint8_t value = cpu_fetch(cpu);
    alu_xor(cpu, value);
}
void opcode_CP_N(CPU *cpu) {
    uint8_t value = cpu_fetch(cpu);
    alu_cp(cpu, value);
}
// 0x07 - RLCA
void opcode_RLCA(CPU *cpu) {
    uint8_t bit7 = (cpu->A >> 7) & 1;
    cpu->A = (cpu->A << 1) | bit7;
    CLEAR_FLAG(cpu, FLAG_Z);
    CLEAR_FLAG(cpu, FLAG_N);
    CLEAR_FLAG(cpu, FLAG_H);
    SET_FLAG_COND(cpu, FLAG_C, bit7);
}


// 0x0F - RRCA
void opcode_RRCA(CPU *cpu) {
    uint8_t bit0 = cpu->A & 1;
    cpu->A = (cpu->A >> 1) | (bit0 << 7);
    CLEAR_FLAG(cpu, FLAG_Z);
    CLEAR_FLAG(cpu, FLAG_N);
    CLEAR_FLAG(cpu, FLAG_H);
    SET_FLAG_COND(cpu, FLAG_C, bit0);
}

// 0x17 - RLA
void opcode_RLA(CPU *cpu) {
    uint8_t bit7 = (cpu->A >> 7) & 1;
    cpu->A = (cpu->A << 1) | GET_FLAG(cpu, FLAG_C);
    CLEAR_FLAG(cpu, FLAG_Z);
    CLEAR_FLAG(cpu, FLAG_N);
    CLEAR_FLAG(cpu, FLAG_H);
    SET_FLAG_COND(cpu, FLAG_C, bit7);
}

// 0x1F - RRA
void opcode_RRA(CPU *cpu) {
    uint8_t bit0 = cpu->A & 1;
    cpu->A = (cpu->A >> 1) | (GET_FLAG(cpu, FLAG_C) << 7);
    CLEAR_FLAG(cpu, FLAG_Z);
    CLEAR_FLAG(cpu, FLAG_N);
    CLEAR_FLAG(cpu, FLAG_H);
    SET_FLAG_COND(cpu, FLAG_C, bit0);
}

///////////////////////////////////////////////////
// EXTRA FUNCTIONS :
///////////////////////////////////////////////////
// === MISC INSTRUCTIONS === //
void opcode_LD_nn_SP(CPU *cpu) {
    uint16_t addr = cpu_fetch(cpu) | (cpu_fetch(cpu) << 8);
    mmu_write8(cpu->mmu, addr, cpu->SP & 0xFF);
    mmu_write8(cpu->mmu, addr + 1, cpu->SP >> 8);
}

void opcode_LD_SP_HL(CPU *cpu) {
    cpu->SP = cpu->HL;
}

void opcode_LD_HL_SP_plus_r8(CPU *cpu) {
    int8_t offset = (int8_t)cpu_fetch(cpu);
    cpu->HL = cpu->SP + offset;

    CLEAR_FLAG(cpu, FLAG_Z);
    CLEAR_FLAG(cpu, FLAG_N);
    SET_FLAG_COND(cpu, FLAG_H, ((cpu->SP & 0x0F) + (offset & 0x0F)) > 0x0F);
    SET_FLAG_COND(cpu, FLAG_C, ((cpu->SP & 0xFF) + (offset & 0xFF)) > 0xFF);
}

void opcode_LDH_n_A(CPU *cpu) {
    uint8_t addr = cpu_fetch(cpu);
    mmu_write8(cpu->mmu, 0xFF00 + addr, cpu->A);
}

void opcode_LDH_A_n(CPU *cpu) {
    uint8_t offset = cpu_fetch(cpu);
    uint16_t full_addr = 0xFF00 + offset;
    uint8_t val = mmu_read8(cpu->mmu, full_addr);
    cpu->A = val;
}
// 16 bit register into A 
void opcode_LD_A_BC(CPU *cpu) {
    cpu->A = mmu_read8(cpu->mmu, cpu->BC);
}
void opcode_LD_A_DE(CPU *cpu) {
    cpu->A = mmu_read8(cpu->mmu, cpu->DE);
}
void opcode_LD_BC_A(CPU *cpu) {
    mmu_write8(cpu->mmu, cpu->BC, cpu->A);
}
void opcode_LD_DE_A(CPU *cpu) {
    mmu_write8(cpu->mmu, cpu->DE, cpu->A);
}
void opcode_LD_E_A(CPU *cpu) {
    // Load C from memory address 0xFF00 + A
    mmu_write8(cpu->mmu, 0xFF00 + cpu->A, cpu->C);
}
void opcode_LD_H_A(CPU *cpu) {
    // Load A from memory address 0xFF00 + C
    cpu->A = mmu_read8(cpu->mmu, 0xFF00 + cpu->C);
}
void opcode_DI(CPU *cpu) {
    cpu->IME = 0;
}

void opcode_EI(CPU *cpu) {
    cpu->ime_pending = 1 ;
}
void opcode_LD_SP_NN(CPU *cpu) {
    uint16_t val = cpu_fetch(cpu);
    val |= (cpu_fetch(cpu) << 8);
    cpu->SP = val;
}

void opcode_LD_HL_NN(CPU *cpu) {
    uint16_t val = cpu_fetch(cpu);
    val |= (cpu_fetch(cpu) << 8);
    cpu->HL = val;
}
void opcode_LD_HL_N(CPU *cpu) {
    uint8_t value = cpu_fetch(cpu);
    mmu_write8(cpu->mmu, cpu->HL, value);
}
void opcode_LD_BC_NN(CPU *cpu) {
    uint16_t val = cpu_fetch(cpu);
    val |= (cpu_fetch(cpu) << 8);
    cpu->BC = val;
}
void opcode_LD_DE_NN(CPU *cpu) {
    uint16_t val = cpu_fetch(cpu);
    val |= (cpu_fetch(cpu) << 8);
    cpu->DE = val;
}
void opcode_ADD_SP_plus_r8(CPU *cpu)
{
    int8_t offset = (int8_t)cpu_fetch(cpu);
    uint16_t sp = cpu->SP;
    uint16_t result = sp + offset;
    CLEAR_FLAG(cpu, FLAG_Z);
    CLEAR_FLAG(cpu, FLAG_N);
    SET_FLAG_COND(cpu, FLAG_H,
        ((sp & 0x0F) + (offset & 0x0F)) > 0x0F);
    SET_FLAG_COND(cpu, FLAG_C,
        ((sp & 0xFF) + (offset & 0xFF)) > 0xFF);
    cpu->SP = result;
}
void opcode_CALL_NN(CPU *cpu) {
    uint16_t addr = cpu_fetch(cpu) | (cpu_fetch(cpu) << 8);
    cpu->SP -= 2;
    mmu_write16(cpu->mmu, cpu->SP, cpu->PC);
    cpu->PC = addr;
}

void opcode_JR_N(CPU *cpu) {
    int8_t offset = (int8_t) cpu_fetch(cpu);
    cpu->PC += offset;
}

void opcode_STOP(CPU *cpu) {
    cpu->halted = 1;
    cpu_fetch(cpu);
}
void opcode_SCF(CPU *cpu) {
    SET_FLAG(cpu, FLAG_C);
    CLEAR_FLAG(cpu, FLAG_N);
    CLEAR_FLAG(cpu, FLAG_H);
}
// PUSH POP :
void opcode_PUSH_BC(CPU *cpu) {
    push16(cpu, cpu->BC);
}
void opcode_PUSH_DE(CPU *cpu) {
    push16(cpu, cpu->DE);
}
void opcode_PUSH_HL(CPU *cpu) {
    push16(cpu, cpu->HL);
}
void opcode_PUSH_AF(CPU *cpu) {
    push16(cpu, cpu->AF);
}
void opcode_POP_BC(CPU *cpu) {
    cpu->BC = pop16(cpu);
}
void opcode_POP_DE(CPU *cpu) {
    cpu->DE = pop16(cpu);
}
void opcode_POP_HL(CPU *cpu) {
    cpu->HL = pop16(cpu);
}
void opcode_POP_AF(CPU *cpu) {
    uint16_t oldAF = cpu->AF;
    cpu->AF = pop16(cpu);
    cpu->F &= 0xF0;
}
// CPL and CCF :
void opcode_CPL(CPU *cpu) {
    cpu->A = ~cpu->A;
    SET_FLAG(cpu, FLAG_N);
    SET_FLAG(cpu, FLAG_H);
}
void opcode_CCF(CPU *cpu) {
    CLEAR_FLAG(cpu, FLAG_N);
    CLEAR_FLAG(cpu, FLAG_H);
    if (GET_FLAG(cpu, FLAG_C)) {
        CLEAR_FLAG(cpu, FLAG_C);
    } else {
        SET_FLAG(cpu, FLAG_C);
    }
}
// DAA:
void opcode_DAA(CPU *cpu) {
    uint8_t correction = 0;
    uint8_t setC = 0;

    if (GET_FLAG(cpu, FLAG_H) || (!GET_FLAG(cpu, FLAG_N) && (cpu->A & 0x0F) > 9)) {
        correction |= 0x06;
    }
    if (GET_FLAG(cpu, FLAG_C) || (!GET_FLAG(cpu, FLAG_N) && cpu->A > 0x99)) {
        correction |= 0x60;
        setC = 1;
    }
    if (GET_FLAG(cpu, FLAG_N)) {
        cpu->A -= correction;
    } else {
        cpu->A += correction;
    }
    SET_FLAG_COND(cpu, FLAG_Z, cpu->A == 0);
    SET_FLAG_COND(cpu, FLAG_C, setC);  // ← properly set OR clear C
    CLEAR_FLAG(cpu, FLAG_H);
}
// SWAP CODES:

// HL ADD operations: 
void opcode_ADD_HL_BC(CPU *cpu) {
    uint32_t result = cpu->HL + cpu->BC;
    cpu->F &= 0x80; // Clear all flags except carry
    SET_FLAG_COND(cpu, FLAG_H, ((cpu->HL & 0x0FFF) + (cpu->BC & 0x0FFF)) > 0x0FFF);
    SET_FLAG_COND(cpu, FLAG_C, result > 0xFFFF);
    cpu->HL = result & 0xFFFF;
}
void opcode_ADD_HL_DE(CPU *cpu) {
    uint32_t result = cpu->HL + cpu->DE;
    cpu->F &= 0x80; // Clear all flags except carry
    SET_FLAG_COND(cpu, FLAG_H, ((cpu->HL & 0x0FFF) + (cpu->DE & 0x0FFF)) > 0x0FFF);
    SET_FLAG_COND(cpu, FLAG_C, result > 0xFFFF);
    cpu->HL = result & 0xFFFF;
}
void opcode_ADD_HL_SP(CPU *cpu) {
    uint32_t result = cpu->HL + cpu->SP;
    cpu->F &= 0x80; // Clear all flags except carry
    SET_FLAG_COND(cpu, FLAG_H, ((cpu->HL & 0x0FFF) + (cpu->SP & 0x0FFF)) > 0x0FFF);
    SET_FLAG_COND(cpu, FLAG_C, result > 0xFFFF);
    cpu->HL = result & 0xFFFF;
}
void opcode_ADD_HL_HL(CPU *cpu) {
    uint32_t result = cpu->HL + cpu->HL;
    cpu->F &= 0x80; // Clear all flags except carry
    SET_FLAG_COND(cpu, FLAG_H, ((cpu->HL & 0x0FFF) + (cpu->HL & 0x0FFF)) > 0x0FFF);
    SET_FLAG_COND(cpu, FLAG_C, result > 0xFFFF);
    cpu->HL = result & 0xFFFF;
}
// === CB PREFIX HANDLER === //
////undocumented CB opcodes:
//-----------------------------------------------
// HELPER MACROS
// -----------------------------------------------

///////////////////////////////////////////////////
// FUNCTION POINTER TABLE HERE: 
///////////////////////////////////////////////////
OpCodeHandler opcode_table[256] ={
    [0x00]= opcode_NOP,
    [0x3E]= opcode_LD_A,
    [0x06]= opcode_LD_B,
    [0x0E]= opcode_LD_C,
    [0x16]= opcode_LD_D,
    [0x1E]= opcode_LD_E,
    [0x26]= opcode_LD_H,
    [0x2E]= opcode_LD_L,
    [0x37] = opcode_SCF,
    // AND OR XOR CP
    [0xA0] = opcode_AND_B,
    [0xA1] = opcode_AND_C,
    [0xA2] = opcode_AND_D,
    [0xA3] = opcode_AND_E,
    [0xA4] = opcode_AND_H,
    [0xA5] = opcode_AND_L,
    [0xA6] = opcode_AND_HL,
    [0xA7] = opcode_AND_A,

    [0xB0] = opcode_OR_B,
    [0xB1] = opcode_OR_C,
    [0xB2] = opcode_OR_D,
    [0xB3] = opcode_OR_E,
    [0xB4] = opcode_OR_H,
    [0xB5] = opcode_OR_L,
    [0xB6] = opcode_OR_HL,
    [0xB7] = opcode_OR_A,

    [0xA8] = opcode_XOR_B,
    [0xA9] = opcode_XOR_C,
    [0xAA] = opcode_XOR_D,
    [0xAB] = opcode_XOR_E,
    [0xAC] = opcode_XOR_H,
    [0xAD] = opcode_XOR_L,
    [0xAE] = opcode_XOR_HL,
    [0xAF] = opcode_XOR_A,

    [0xB8] = opcode_CP_B,
    [0xB9] = opcode_CP_C,
    [0xBA] = opcode_CP_D,
    [0xBB] = opcode_CP_E,
    [0xBC] = opcode_CP_H,
    [0xBD] = opcode_CP_L,
    [0xBE] = opcode_CP_HL,
    [0xBF] = opcode_CP_A,

    // A
    [0x7F]= opcode_LD_AA,
    [0x78]= opcode_LD_AB,
    [0x79]= opcode_LD_AC,
    [0x7A]= opcode_LD_AD,
    [0x7B]= opcode_LD_AE,
    [0x7C]= opcode_LD_AH,
    [0x7D]= opcode_LD_AL,
    // B
    [0x47]= opcode_LD_BA,
    [0x40]= opcode_LD_BB,
    [0x41]= opcode_LD_BC,
    [0x42]= opcode_LD_BD,
    [0x43]= opcode_LD_BE,
    [0x44]= opcode_LD_BH,
    [0x45]= opcode_LD_BL,
    // C
    [0x4F]= opcode_LD_CA,
    [0x48]= opcode_LD_CB,
    [0x49]= opcode_LD_CC,
    [0x4A]= opcode_LD_CD,
    [0x4B]= opcode_LD_CE,
    [0x4C]= opcode_LD_CH,
    [0x4D]= opcode_LD_CL,
    // D
    [0x57]= opcode_LD_DA,
    [0x50]= opcode_LD_DB,
    [0x51]= opcode_LD_DC,
    [0x52]= opcode_LD_DD,
    [0x53]= opcode_LD_DE,
    [0x54]= opcode_LD_DH,
    [0x55]= opcode_LD_DL,
    // E
    [0x5F]= opcode_LD_EA,
    [0x58]= opcode_LD_EB,
    [0x59]= opcode_LD_EC,
    [0x5A]= opcode_LD_ED,
    [0x5B]= opcode_LD_EE,
    [0x5C]= opcode_LD_EH,
    [0x5D]= opcode_LD_EL,
    // H
    [0x67]= opcode_LD_HA,
    [0x60]= opcode_LD_HB,
    [0x61]= opcode_LD_HC,
    [0x62]= opcode_LD_HD,
    [0x63]= opcode_LD_HE,
    [0x64]= opcode_LD_HH,
    [0x65]= opcode_LD_HL,
    // L
    [0x6F]= opcode_LD_LA,
    [0x68]= opcode_LD_LB,
    [0x69]= opcode_LD_LC,
    [0x6A]= opcode_LD_LD,
    [0x6B]= opcode_LD_LE,
    [0x6C]= opcode_LD_LH,
    [0x6D]= opcode_LD_LL,
    // HL TO REGISTERS:
    [0x77]= opcode_LD_HL_A,
    [0x70]= opcode_LD_HL_B,
    [0x71]= opcode_LD_HL_C,
    [0x72]= opcode_LD_HL_D,
    [0x73]= opcode_LD_HL_E,
    [0x74]= opcode_LD_HL_H,
    [0x75]= opcode_LD_HL_L,
    // REGISTERS TO HL:
    [0x7E]= opcode_LD_A_HL,
    [0x46]= opcode_LD_B_HL,
    [0x4E]= opcode_LD_C_HL,
    [0x56]= opcode_LD_D_HL,
    [0x5E]= opcode_LD_E_HL,
    [0x66]= opcode_LD_H_HL,
    [0x6E]= opcode_LD_L_HL,
    // C/NN INTO A:
    [0xF2]= opcode_LD_A_C,
    [0xFA]= opcode_LD_A_NN,
    // A INTO C/NN:
    [0xE2]= opcode_LD_C_A,
    [0xEA]= opcode_LD_NN_A,
    [0xE9] = opcode_JP_HL,
    // H+, H- INTO A
    [0x76]= opcode_HALT,
    [0x2A] = opcode_LD_A_HL_inc,
    [0x3A] = opcode_LD_A_HL_dec,
    [0x22] = opcode_LD_HL_inc_A,
    [0x32] = opcode_LD_HL_dec_A,
    // ADDITION : 
    [0x80]= opcode_ADD_A_B,
    [0x81]= opcode_ADD_A_C,
    [0x82]= opcode_ADD_A_D,
    [0x83]= opcode_ADD_A_E,
    [0x84]= opcode_ADD_A_H,
    [0x85]= opcode_ADD_A_L,
    [0x86]= opcode_ADD_A_HL,
    [0x87]= opcode_ADD_A_A,
    //SUBTRACTION :
    [0x90] = opcode_SUB_A_B,
    [0x91] = opcode_SUB_A_C,
    [0x92] = opcode_SUB_A_D,
    [0x93] = opcode_SUB_A_E,
    [0x94] = opcode_SUB_A_H,
    [0x95] = opcode_SUB_A_L,
    [0x96] = opcode_SUB_A_HL,
    [0x97] = opcode_SUB_A_A,
    //ADC :
    [0x88]= opcode_ADC_A_B,
    [0x89]= opcode_ADC_A_C,
    [0x8A]= opcode_ADC_A_D,
    [0x8B]= opcode_ADC_A_E,
    [0x8C]= opcode_ADC_A_H,
    [0x8D]= opcode_ADC_A_L,
    [0x8E]= opcode_ADC_A_HL,
    [0x8F]= opcode_ADC_A_A,
    //SBC :
    [0x98] = opcode_SBC_A_B,
    [0x99] = opcode_SBC_A_C,
    [0x9A] = opcode_SBC_A_D,
    [0x9B] = opcode_SBC_A_E,
    [0x9C] = opcode_SBC_A_H,
    [0x9D] = opcode_SBC_A_L,
    [0x9E] = opcode_SBC_A_HL,
    [0x9F] = opcode_SBC_A_A,
    // INC DEC :
    //// INC : 
    [0x04] = opcode_INC_B,
    [0x0C] = opcode_INC_C,
    [0x14] = opcode_INC_D,
    [0x1C] = opcode_INC_E,
    [0x24] = opcode_INC_H,
    [0x2C] = opcode_INC_L,
    [0x34] = opcode_INC_HL,
    [0x3C] = opcode_INC_A,
    //// DEC :
    [0x05] = opcode_DEC_B,
    [0x0D] = opcode_DEC_C,
    [0x15] = opcode_DEC_D,
    [0x1D] = opcode_DEC_E,
    [0x25] = opcode_DEC_H,
    [0x2D] = opcode_DEC_L,
    [0x35] = opcode_DEC_HL,
    [0x3D] = opcode_DEC_A,
    // 16-bit INC DEC :
    //// INC : 
    [0x03] = opcode_INC_BC,
    [0x13] = opcode_INC_DE,
    [0x23] = opcode_INC_HL_16,
    [0x33] = opcode_INC_SP,
    //// DEC :
    [0x0B] = opcode_DEC_BC,
    [0x1B] = opcode_DEC_DE,
    [0x2B] = opcode_DEC_HL_16,
    [0x3B] = opcode_DEC_SP,
    // JUMP RET RETI CALL :
    //// JUMP : 
    [0xC3] = opcode_JP,
    [0xC2] = opcode_JP_NZ,
    [0xCA] = opcode_JP_Z,
    [0xD2] = opcode_JP_NC,
    [0xDA] = opcode_JP_C,
    //// JUMP REGISTER :
    [0x18] = opcode_JR,
    [0x20] = opcode_JR_NZ,
    [0x28] = opcode_JR_Z,
    [0x30] = opcode_JR_NC,
    [0x38] = opcode_JR_C,
    //// CALL : 
    [0xCD] = opcode_CALL,
    [0xC4] = opcode_CALL_NZ,
    [0xCC] = opcode_CALL_Z,
    [0xD4] = opcode_CALL_NC,
    [0xDC] = opcode_CALL_C,
    //// RET : 
    [0xC9] = opcode_RET,
    [0xC0] = opcode_RET_NZ,
    [0xC8] = opcode_RET_Z,
    [0xD0] = opcode_RET_NC,
    [0xD8] = opcode_RET_C,
    //// RETI : 
    [0xD9] = opcode_RETI,
    //// RST :
    [0xC7] = opcode_RST_00,
    [0xCF] = opcode_RST_08,
    [0xD7] = opcode_RST_10,
    [0xDF] = opcode_RST_18,
    [0xE7] = opcode_RST_20,
    [0xEF] = opcode_RST_28,
    [0xF7] = opcode_RST_30,
    [0xFF] = opcode_RST_38,
    [0x08] = opcode_LD_nn_SP,
    [0xF9] = opcode_LD_SP_HL,
    [0xF8] = opcode_LD_HL_SP_plus_r8,
    [0xE8] = opcode_ADD_SP_plus_r8, // Undocumented but behaves the same as 0xF8
    [0xE0] = opcode_LDH_n_A,
    [0xF0] = opcode_LDH_A_n,
    [0xF3] = opcode_DI,
    [0xFB] = opcode_EI,
    [0x31] = opcode_LD_SP_NN,
    [0x21] = opcode_LD_HL_NN,
    [0xE5] = opcode_PUSH_HL,
    [0xC5] = opcode_PUSH_BC,
    [0xD5] = opcode_PUSH_DE,
    [0xF5] = opcode_PUSH_AF,
    [0xE1] = opcode_POP_HL,
    [0xC1] = opcode_POP_BC,
    [0xD1] = opcode_POP_DE,
    [0xF1] = opcode_POP_AF,
    [0x01] = opcode_LD_BC_NN,
    [0x11] = opcode_LD_DE_NN,
    [0xE6] = opcode_AND_N,
    [0xF6] = opcode_OR_N,
    [0xEE] = opcode_XOR_N,
    [0xFE] = opcode_CP_N,
    [0x12] = opcode_LD_DE_A,
    [0x1A] = opcode_LD_A_DE,
    [0x02] = opcode_LD_BC_A,
    [0x0A] = opcode_LD_A_BC,
    // CB-prefix dispatch (handled separately)
    [0xCB] = opcode_CB_dispatch,
    // STOP
    [0x10] = opcode_STOP,
    // OPS from N 
    [0xD6] = opcode_SUB_N,
    [0xC6] = opcode_ADD_N,      // ADD A, n
    [0xCE] = opcode_ADC_N,      // ADC A, n
    [0xDE] = opcode_SBC_N,     // SBC A, n
    [0x27] = opcode_DAA,        // DAA
    [0x2F] = opcode_CPL,        // CPL
    [0x3F] = opcode_CCF,        // CCF
    // ALL HL opcodes including ADD
    [0x09] = opcode_ADD_HL_BC,
    [0x19] = opcode_ADD_HL_DE,
    [0x29] = opcode_ADD_HL_HL,
    [0x39] = opcode_ADD_HL_SP,
    [0x36] = opcode_LD_HL_N,
    [0x07] = opcode_RLCA,
    [0x0F] = opcode_RRCA,
    [0x17] = opcode_RLA,
    [0x1F] = opcode_RRA,

};
///////////////////////////////////////////////////
// CB OPCODES
///////////////////////////////////////////////////

// RLC
#define IMPL_RLC(reg) \
static inline void opcode_RLC_##reg(CPU *cpu) { \
    uint8_t bit7 = (cpu->reg >> 7) & 1; \
    cpu->reg = (cpu->reg << 1) | bit7; \
    cpu->F = 0; \
    SET_FLAG_COND(cpu, FLAG_C, bit7); \
    SET_FLAG_COND(cpu, FLAG_Z, cpu->reg == 0); \
}
IMPL_RLC(A) IMPL_RLC(B) IMPL_RLC(C) IMPL_RLC(D)
IMPL_RLC(E) IMPL_RLC(H) IMPL_RLC(L)
static inline void opcode_RLC_HL(CPU *cpu) {
    uint8_t val = mmu_read8(cpu->mmu, cpu->HL);
    uint8_t bit7 = (val >> 7) & 1;
    val = (val << 1) | bit7;
    mmu_write8(cpu->mmu, cpu->HL, val);
    cpu->F = 0;
    SET_FLAG_COND(cpu, FLAG_C, bit7);
    SET_FLAG_COND(cpu, FLAG_Z, val == 0);
}

// RRC
#define IMPL_RRC(reg) \
static inline void opcode_RRC_##reg(CPU *cpu) { \
    uint8_t bit0 = cpu->reg & 1; \
    cpu->reg = (cpu->reg >> 1) | (bit0 << 7); \
    cpu->F = 0; \
    SET_FLAG_COND(cpu, FLAG_C, bit0); \
    SET_FLAG_COND(cpu, FLAG_Z, cpu->reg == 0); \
}
IMPL_RRC(A) IMPL_RRC(B) IMPL_RRC(C) IMPL_RRC(D)
IMPL_RRC(E) IMPL_RRC(H) IMPL_RRC(L)
static inline void opcode_RRC_HL(CPU *cpu) {
    uint8_t val = mmu_read8(cpu->mmu, cpu->HL);
    uint8_t bit0 = val & 1;
    val = (val >> 1) | (bit0 << 7);
    mmu_write8(cpu->mmu, cpu->HL, val);
    cpu->F = 0;
    SET_FLAG_COND(cpu, FLAG_C, bit0);
    SET_FLAG_COND(cpu, FLAG_Z, val == 0);
}

// RL
#define IMPL_RL(reg) \
static inline void opcode_RL_##reg(CPU *cpu) { \
    uint8_t bit7 = (cpu->reg >> 7) & 1; \
    uint8_t old_carry = GET_FLAG(cpu, FLAG_C); \
    cpu->reg = (cpu->reg << 1) | old_carry; \
    cpu->F = 0; \
    SET_FLAG_COND(cpu, FLAG_C, bit7); \
    SET_FLAG_COND(cpu, FLAG_Z, cpu->reg == 0); \
}
IMPL_RL(A) IMPL_RL(B) IMPL_RL(C) IMPL_RL(D)
IMPL_RL(E) IMPL_RL(H) IMPL_RL(L)
static inline void opcode_RL_HL(CPU *cpu) {
    uint8_t val = mmu_read8(cpu->mmu, cpu->HL);
    uint8_t bit7 = (val >> 7) & 1;
    uint8_t old_carry = GET_FLAG(cpu, FLAG_C);
    val = (val << 1) | old_carry;
    mmu_write8(cpu->mmu, cpu->HL, val);
    cpu->F = 0;
    SET_FLAG_COND(cpu, FLAG_C, bit7);
    SET_FLAG_COND(cpu, FLAG_Z, val == 0);
}

// RR
#define IMPL_RR(reg) \
static inline void opcode_RR_##reg(CPU *cpu) { \
    uint8_t bit0 = cpu->reg & 1; \
    uint8_t old_carry = GET_FLAG(cpu, FLAG_C); \
    cpu->reg = (cpu->reg >> 1) | (old_carry << 7); \
    cpu->F = 0; \
    SET_FLAG_COND(cpu, FLAG_C, bit0); \
    SET_FLAG_COND(cpu, FLAG_Z, cpu->reg == 0); \
}
IMPL_RR(A) IMPL_RR(B) IMPL_RR(C) IMPL_RR(D)
IMPL_RR(E) IMPL_RR(H) IMPL_RR(L)
static inline void opcode_RR_HL(CPU *cpu) {
    uint8_t val = mmu_read8(cpu->mmu, cpu->HL);
    uint8_t bit0 = val & 1;
    uint8_t old_carry = GET_FLAG(cpu, FLAG_C);
    val = (val >> 1) | (old_carry << 7);
    mmu_write8(cpu->mmu, cpu->HL, val);
    cpu->F = 0;
    SET_FLAG_COND(cpu, FLAG_C, bit0);
    SET_FLAG_COND(cpu, FLAG_Z, val == 0);
}

// SLA
#define IMPL_SLA(reg) \
static inline void opcode_SLA_##reg(CPU *cpu) { \
    uint8_t bit7 = (cpu->reg >> 7) & 1; \
    cpu->reg <<= 1; \
    cpu->F = 0; \
    SET_FLAG_COND(cpu, FLAG_C, bit7); \
    SET_FLAG_COND(cpu, FLAG_Z, cpu->reg == 0); \
}
IMPL_SLA(A) IMPL_SLA(B) IMPL_SLA(C) IMPL_SLA(D)
IMPL_SLA(E) IMPL_SLA(H) IMPL_SLA(L)
static inline void opcode_SLA_HL(CPU *cpu) {
    uint8_t val = mmu_read8(cpu->mmu, cpu->HL);
    uint8_t bit7 = (val >> 7) & 1;
    val <<= 1;
    mmu_write8(cpu->mmu, cpu->HL, val);
    cpu->F = 0;
    SET_FLAG_COND(cpu, FLAG_C, bit7);
    SET_FLAG_COND(cpu, FLAG_Z, val == 0);
}

// SRA
#define IMPL_SRA(reg) \
static inline void opcode_SRA_##reg(CPU *cpu) { \
    uint8_t bit0 = cpu->reg & 1; \
    uint8_t bit7 = cpu->reg & 0x80; \
    cpu->reg = (cpu->reg >> 1) | bit7; \
    cpu->F = 0; \
    SET_FLAG_COND(cpu, FLAG_C, bit0); \
    SET_FLAG_COND(cpu, FLAG_Z, cpu->reg == 0); \
}
IMPL_SRA(A) IMPL_SRA(B) IMPL_SRA(C) IMPL_SRA(D)
IMPL_SRA(E) IMPL_SRA(H) IMPL_SRA(L)
static inline void opcode_SRA_HL(CPU *cpu) {
    uint8_t val = mmu_read8(cpu->mmu, cpu->HL);
    uint8_t bit0 = val & 1;
    uint8_t bit7 = val & 0x80;
    val = (val >> 1) | bit7;
    mmu_write8(cpu->mmu, cpu->HL, val);
    cpu->F = 0;
    SET_FLAG_COND(cpu, FLAG_C, bit0);
    SET_FLAG_COND(cpu, FLAG_Z, val == 0);
}

// SWAP
#define IMPL_SWAP(reg) \
static inline void opcode_SWAP_##reg(CPU *cpu) { \
    cpu->reg = ((cpu->reg & 0x0F) << 4) | ((cpu->reg & 0xF0) >> 4); \
    cpu->F = 0; \
    SET_FLAG_COND(cpu, FLAG_Z, cpu->reg == 0); \
}
IMPL_SWAP(A) IMPL_SWAP(B) IMPL_SWAP(C) IMPL_SWAP(D)
IMPL_SWAP(E) IMPL_SWAP(H) IMPL_SWAP(L)
static inline void opcode_SWAP_HL(CPU *cpu) {
    uint8_t val = mmu_read8(cpu->mmu, cpu->HL);
    val = ((val & 0x0F) << 4) | ((val & 0xF0) >> 4);
    mmu_write8(cpu->mmu, cpu->HL, val);
    cpu->F = 0;
    SET_FLAG_COND(cpu, FLAG_Z, val == 0);
}

// SRL
#define IMPL_SRL(reg) \
static inline void opcode_SRL_##reg(CPU *cpu) { \
    uint8_t bit0 = cpu->reg & 1; \
    cpu->reg >>= 1; \
    cpu->F = 0; \
    SET_FLAG_COND(cpu, FLAG_C, bit0); \
    SET_FLAG_COND(cpu, FLAG_Z, cpu->reg == 0); \
}
IMPL_SRL(A) IMPL_SRL(B) IMPL_SRL(C) IMPL_SRL(D)
IMPL_SRL(E) IMPL_SRL(H) IMPL_SRL(L)
static inline void opcode_SRL_HL(CPU *cpu) {
    uint8_t val = mmu_read8(cpu->mmu, cpu->HL);
    uint8_t bit0 = val & 1;
    val >>= 1;
    mmu_write8(cpu->mmu, cpu->HL, val);
    cpu->F = 0;
    SET_FLAG_COND(cpu, FLAG_C, bit0);
    SET_FLAG_COND(cpu, FLAG_Z, val == 0);
}

// BIT
#define IMPL_BIT(n, reg) \
static inline void opcode_BIT_##n##_##reg(CPU *cpu) { \
    CLEAR_FLAG(cpu, FLAG_N); \
    SET_FLAG(cpu, FLAG_H); \
    SET_FLAG_COND(cpu, FLAG_Z, !(cpu->reg & (1 << n))); \
}
#define IMPL_BIT_HL(n) \
static inline void opcode_BIT_##n##_HL(CPU *cpu) { \
    uint8_t val = mmu_read8(cpu->mmu, cpu->HL); \
    CLEAR_FLAG(cpu, FLAG_N); \
    SET_FLAG(cpu, FLAG_H); \
    SET_FLAG_COND(cpu, FLAG_Z, !(val & (1 << n))); \
}
#define IMPL_BIT_ROW(n) \
    IMPL_BIT(n,B) IMPL_BIT(n,C) IMPL_BIT(n,D) IMPL_BIT(n,E) \
    IMPL_BIT(n,H) IMPL_BIT(n,L) IMPL_BIT(n,A) IMPL_BIT_HL(n)
IMPL_BIT_ROW(0) IMPL_BIT_ROW(1) IMPL_BIT_ROW(2) IMPL_BIT_ROW(3)
IMPL_BIT_ROW(4) IMPL_BIT_ROW(5) IMPL_BIT_ROW(6) IMPL_BIT_ROW(7)

// RES
#define IMPL_RES(n, reg) \
static inline void opcode_RES_##n##_##reg(CPU *cpu) { cpu->reg &= ~(1 << n); }
#define IMPL_RES_HL(n) \
static inline void opcode_RES_##n##_HL(CPU *cpu) { \
    mmu_write8(cpu->mmu, cpu->HL, mmu_read8(cpu->mmu, cpu->HL) & ~(1 << n)); \
}
#define IMPL_RES_ROW(n) \
    IMPL_RES(n,B) IMPL_RES(n,C) IMPL_RES(n,D) IMPL_RES(n,E) \
    IMPL_RES(n,H) IMPL_RES(n,L) IMPL_RES(n,A) IMPL_RES_HL(n)
IMPL_RES_ROW(0) IMPL_RES_ROW(1) IMPL_RES_ROW(2) IMPL_RES_ROW(3)
IMPL_RES_ROW(4) IMPL_RES_ROW(5) IMPL_RES_ROW(6) IMPL_RES_ROW(7)

// SET
#define IMPL_SET(n, reg) \
static inline void opcode_SET_##n##_##reg(CPU *cpu) { cpu->reg |= (1 << n); }
#define IMPL_SET_HL(n) \
static inline void opcode_SET_##n##_HL(CPU *cpu) { \
    mmu_write8(cpu->mmu, cpu->HL, mmu_read8(cpu->mmu, cpu->HL) | (1 << n)); \
}
#define IMPL_SET_ROW(n) \
    IMPL_SET(n,B) IMPL_SET(n,C) IMPL_SET(n,D) IMPL_SET(n,E) \
    IMPL_SET(n,H) IMPL_SET(n,L) IMPL_SET(n,A) IMPL_SET_HL(n)
IMPL_SET_ROW(0) IMPL_SET_ROW(1) IMPL_SET_ROW(2) IMPL_SET_ROW(3)
IMPL_SET_ROW(4) IMPL_SET_ROW(5) IMPL_SET_ROW(6) IMPL_SET_ROW(7)

///////////////////////////////////////////////////
// CB OPCODE TABLE
///////////////////////////////////////////////////
OpCodeHandler cb_opcode_table[256] = {
    // RLC
    [0x00]=opcode_RLC_B, [0x01]=opcode_RLC_C, [0x02]=opcode_RLC_D, [0x03]=opcode_RLC_E,
    [0x04]=opcode_RLC_H, [0x05]=opcode_RLC_L, [0x06]=opcode_RLC_HL,[0x07]=opcode_RLC_A,
    // RRC
    [0x08]=opcode_RRC_B, [0x09]=opcode_RRC_C, [0x0A]=opcode_RRC_D, [0x0B]=opcode_RRC_E,
    [0x0C]=opcode_RRC_H, [0x0D]=opcode_RRC_L, [0x0E]=opcode_RRC_HL,[0x0F]=opcode_RRC_A,
    // RL
    [0x10]=opcode_RL_B,  [0x11]=opcode_RL_C,  [0x12]=opcode_RL_D,  [0x13]=opcode_RL_E,
    [0x14]=opcode_RL_H,  [0x15]=opcode_RL_L,  [0x16]=opcode_RL_HL, [0x17]=opcode_RL_A,
    // RR
    [0x18]=opcode_RR_B,  [0x19]=opcode_RR_C,  [0x1A]=opcode_RR_D,  [0x1B]=opcode_RR_E,
    [0x1C]=opcode_RR_H,  [0x1D]=opcode_RR_L,  [0x1E]=opcode_RR_HL, [0x1F]=opcode_RR_A,
    // SLA
    [0x20]=opcode_SLA_B, [0x21]=opcode_SLA_C, [0x22]=opcode_SLA_D, [0x23]=opcode_SLA_E,
    [0x24]=opcode_SLA_H, [0x25]=opcode_SLA_L, [0x26]=opcode_SLA_HL,[0x27]=opcode_SLA_A,
    // SRA
    [0x28]=opcode_SRA_B, [0x29]=opcode_SRA_C, [0x2A]=opcode_SRA_D, [0x2B]=opcode_SRA_E,
    [0x2C]=opcode_SRA_H, [0x2D]=opcode_SRA_L, [0x2E]=opcode_SRA_HL,[0x2F]=opcode_SRA_A,
    // SWAP
    [0x30]=opcode_SWAP_B,[0x31]=opcode_SWAP_C,[0x32]=opcode_SWAP_D,[0x33]=opcode_SWAP_E,
    [0x34]=opcode_SWAP_H,[0x35]=opcode_SWAP_L,[0x36]=opcode_SWAP_HL,[0x37]=opcode_SWAP_A,
    // SRL
    [0x38]=opcode_SRL_B, [0x39]=opcode_SRL_C, [0x3A]=opcode_SRL_D, [0x3B]=opcode_SRL_E,
    [0x3C]=opcode_SRL_H, [0x3D]=opcode_SRL_L, [0x3E]=opcode_SRL_HL,[0x3F]=opcode_SRL_A,
    // BIT 0-7
    [0x40]=opcode_BIT_0_B, [0x41]=opcode_BIT_0_C, [0x42]=opcode_BIT_0_D, [0x43]=opcode_BIT_0_E,
    [0x44]=opcode_BIT_0_H, [0x45]=opcode_BIT_0_L, [0x46]=opcode_BIT_0_HL,[0x47]=opcode_BIT_0_A,
    [0x48]=opcode_BIT_1_B, [0x49]=opcode_BIT_1_C, [0x4A]=opcode_BIT_1_D, [0x4B]=opcode_BIT_1_E,
    [0x4C]=opcode_BIT_1_H, [0x4D]=opcode_BIT_1_L, [0x4E]=opcode_BIT_1_HL,[0x4F]=opcode_BIT_1_A,
    [0x50]=opcode_BIT_2_B, [0x51]=opcode_BIT_2_C, [0x52]=opcode_BIT_2_D, [0x53]=opcode_BIT_2_E,
    [0x54]=opcode_BIT_2_H, [0x55]=opcode_BIT_2_L, [0x56]=opcode_BIT_2_HL,[0x57]=opcode_BIT_2_A,
    [0x58]=opcode_BIT_3_B, [0x59]=opcode_BIT_3_C, [0x5A]=opcode_BIT_3_D, [0x5B]=opcode_BIT_3_E,
    [0x5C]=opcode_BIT_3_H, [0x5D]=opcode_BIT_3_L, [0x5E]=opcode_BIT_3_HL,[0x5F]=opcode_BIT_3_A,
    [0x60]=opcode_BIT_4_B, [0x61]=opcode_BIT_4_C, [0x62]=opcode_BIT_4_D, [0x63]=opcode_BIT_4_E,
    [0x64]=opcode_BIT_4_H, [0x65]=opcode_BIT_4_L, [0x66]=opcode_BIT_4_HL,[0x67]=opcode_BIT_4_A,
    [0x68]=opcode_BIT_5_B, [0x69]=opcode_BIT_5_C, [0x6A]=opcode_BIT_5_D, [0x6B]=opcode_BIT_5_E,
    [0x6C]=opcode_BIT_5_H, [0x6D]=opcode_BIT_5_L, [0x6E]=opcode_BIT_5_HL,[0x6F]=opcode_BIT_5_A,
    [0x70]=opcode_BIT_6_B, [0x71]=opcode_BIT_6_C, [0x72]=opcode_BIT_6_D, [0x73]=opcode_BIT_6_E,
    [0x74]=opcode_BIT_6_H, [0x75]=opcode_BIT_6_L, [0x76]=opcode_BIT_6_HL,[0x77]=opcode_BIT_6_A,
    [0x78]=opcode_BIT_7_B, [0x79]=opcode_BIT_7_C, [0x7A]=opcode_BIT_7_D, [0x7B]=opcode_BIT_7_E,
    [0x7C]=opcode_BIT_7_H, [0x7D]=opcode_BIT_7_L, [0x7E]=opcode_BIT_7_HL,[0x7F]=opcode_BIT_7_A,
    // RES 0-7
    [0x80]=opcode_RES_0_B, [0x81]=opcode_RES_0_C, [0x82]=opcode_RES_0_D, [0x83]=opcode_RES_0_E,
    [0x84]=opcode_RES_0_H, [0x85]=opcode_RES_0_L, [0x86]=opcode_RES_0_HL,[0x87]=opcode_RES_0_A,
    [0x88]=opcode_RES_1_B, [0x89]=opcode_RES_1_C, [0x8A]=opcode_RES_1_D, [0x8B]=opcode_RES_1_E,
    [0x8C]=opcode_RES_1_H, [0x8D]=opcode_RES_1_L, [0x8E]=opcode_RES_1_HL,[0x8F]=opcode_RES_1_A,
    [0x90]=opcode_RES_2_B, [0x91]=opcode_RES_2_C, [0x92]=opcode_RES_2_D, [0x93]=opcode_RES_2_E,
    [0x94]=opcode_RES_2_H, [0x95]=opcode_RES_2_L, [0x96]=opcode_RES_2_HL,[0x97]=opcode_RES_2_A,
    [0x98]=opcode_RES_3_B, [0x99]=opcode_RES_3_C, [0x9A]=opcode_RES_3_D, [0x9B]=opcode_RES_3_E,
    [0x9C]=opcode_RES_3_H, [0x9D]=opcode_RES_3_L, [0x9E]=opcode_RES_3_HL,[0x9F]=opcode_RES_3_A,
    [0xA0]=opcode_RES_4_B, [0xA1]=opcode_RES_4_C, [0xA2]=opcode_RES_4_D, [0xA3]=opcode_RES_4_E,
    [0xA4]=opcode_RES_4_H, [0xA5]=opcode_RES_4_L, [0xA6]=opcode_RES_4_HL,[0xA7]=opcode_RES_4_A,
    [0xA8]=opcode_RES_5_B, [0xA9]=opcode_RES_5_C, [0xAA]=opcode_RES_5_D, [0xAB]=opcode_RES_5_E,
    [0xAC]=opcode_RES_5_H, [0xAD]=opcode_RES_5_L, [0xAE]=opcode_RES_5_HL,[0xAF]=opcode_RES_5_A,
    [0xB0]=opcode_RES_6_B, [0xB1]=opcode_RES_6_C, [0xB2]=opcode_RES_6_D, [0xB3]=opcode_RES_6_E,
    [0xB4]=opcode_RES_6_H, [0xB5]=opcode_RES_6_L, [0xB6]=opcode_RES_6_HL,[0xB7]=opcode_RES_6_A,
    [0xB8]=opcode_RES_7_B, [0xB9]=opcode_RES_7_C, [0xBA]=opcode_RES_7_D, [0xBB]=opcode_RES_7_E,
    [0xBC]=opcode_RES_7_H, [0xBD]=opcode_RES_7_L, [0xBE]=opcode_RES_7_HL,[0xBF]=opcode_RES_7_A,
    // SET 0-7
    [0xC0]=opcode_SET_0_B, [0xC1]=opcode_SET_0_C, [0xC2]=opcode_SET_0_D, [0xC3]=opcode_SET_0_E,
    [0xC4]=opcode_SET_0_H, [0xC5]=opcode_SET_0_L, [0xC6]=opcode_SET_0_HL,[0xC7]=opcode_SET_0_A,
    [0xC8]=opcode_SET_1_B, [0xC9]=opcode_SET_1_C, [0xCA]=opcode_SET_1_D, [0xCB]=opcode_SET_1_E,
    [0xCC]=opcode_SET_1_H, [0xCD]=opcode_SET_1_L, [0xCE]=opcode_SET_1_HL,[0xCF]=opcode_SET_1_A,
    [0xD0]=opcode_SET_2_B, [0xD1]=opcode_SET_2_C, [0xD2]=opcode_SET_2_D, [0xD3]=opcode_SET_2_E,
    [0xD4]=opcode_SET_2_H, [0xD5]=opcode_SET_2_L, [0xD6]=opcode_SET_2_HL,[0xD7]=opcode_SET_2_A,
    [0xD8]=opcode_SET_3_B, [0xD9]=opcode_SET_3_C, [0xDA]=opcode_SET_3_D, [0xDB]=opcode_SET_3_E,
    [0xDC]=opcode_SET_3_H, [0xDD]=opcode_SET_3_L, [0xDE]=opcode_SET_3_HL,[0xDF]=opcode_SET_3_A,
    [0xE0]=opcode_SET_4_B, [0xE1]=opcode_SET_4_C, [0xE2]=opcode_SET_4_D, [0xE3]=opcode_SET_4_E,
    [0xE4]=opcode_SET_4_H, [0xE5]=opcode_SET_4_L, [0xE6]=opcode_SET_4_HL,[0xE7]=opcode_SET_4_A,
    [0xE8]=opcode_SET_5_B, [0xE9]=opcode_SET_5_C, [0xEA]=opcode_SET_5_D, [0xEB]=opcode_SET_5_E,
    [0xEC]=opcode_SET_5_H, [0xED]=opcode_SET_5_L, [0xEE]=opcode_SET_5_HL,[0xEF]=opcode_SET_5_A,
    [0xF0]=opcode_SET_6_B, [0xF1]=opcode_SET_6_C, [0xF2]=opcode_SET_6_D, [0xF3]=opcode_SET_6_E,
    [0xF4]=opcode_SET_6_H, [0xF5]=opcode_SET_6_L, [0xF6]=opcode_SET_6_HL,[0xF7]=opcode_SET_6_A,
    [0xF8]=opcode_SET_7_B, [0xF9]=opcode_SET_7_C, [0xFA]=opcode_SET_7_D, [0xFB]=opcode_SET_7_E,
    [0xFC]=opcode_SET_7_H, [0xFD]=opcode_SET_7_L, [0xFE]=opcode_SET_7_HL,[0xFF]=opcode_SET_7_A,
};
void log_cpu_state(CPU *cpu) {
    fprintf(logfile, "A:%02X F:%02X B:%02X C:%02X D:%02X E:%02X H:%02X L:%02X SP:%04X PC:%04X PCMEM:%02X,%02X,%02X,%02X\n",
        cpu->A, cpu->F, cpu->B, cpu->C, cpu->D, cpu->E, cpu->H, cpu->L, cpu->SP, cpu->PC,
        mmu_read8(cpu->mmu, cpu->PC),
        mmu_read8(cpu->mmu, cpu->PC+1),
        mmu_read8(cpu->mmu, cpu->PC+2),
        mmu_read8(cpu->mmu, cpu->PC+3)
    );
}
// ------------------------------------------------
// Per-opcode timing (machine cycles, 1 MC = 4 dots)
// Conditional branches use the NOT-taken value.
// ------------------------------------------------
static const uint8_t opcode_cycles[256] = {
    // 0x00 - 0x3F
    1,3,2,2,1,1,2,1,  5,2,2,2,1,1,2,1,
    1,3,2,2,1,1,2,1,  3,2,2,2,1,1,2,1,
    2,3,2,2,1,1,2,1,  2,2,2,2,1,1,2,1,
    2,3,2,2,3,3,3,1,  2,2,2,2,1,1,2,1,
    // 0x40 - 0x7F
    1,1,1,1,1,1,2,1,  1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,  1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,  2,2,2,2,2,2,1,2,
    1,1,1,1,1,1,2,1,  1,1,1,1,1,1,2,1,
    // 0x80 - 0xBF
    1,1,1,1,1,1,2,1,  1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,  1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,  1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,  1,1,1,1,1,1,2,1,
    // 0xC0 - 0xFF
    2,3,3,4,3,4,2,4,  2,4,3,2,3,6,2,4,
    2,3,3,0,3,4,2,4,  2,4,3,0,3,0,2,4,
    3,3,2,0,0,4,2,4,  4,1,4,0,0,0,2,4,
    3,3,2,1,0,4,2,4,  3,2,4,1,0,0,2,4
};

// CB prefix: all take 2 MC except the (HL) forms which take 4.
static uint8_t cb_opcode_cycles[256];
static void init_cb_cycles(void) {
    for (int i = 0; i < 256; i++) cb_opcode_cycles[i] = 2;
    static const uint8_t hl_ops[] = {
        0x06,0x0E,0x16,0x1E,0x26,0x2E,0x36,0x3E,
        0x46,0x4E,0x56,0x5E,0x66,0x6E,0x76,0x7E,
        0x86,0x8E,0x96,0x9E,0xA6,0xAE,0xB6,0xBE,
        0xC6,0xCE,0xD6,0xDE,0xE6,0xEE,0xF6,0xFE
    };
    for (int i = 0; i < 32; i++) cb_opcode_cycles[hl_ops[i]] = 4;
}

void opcode_CB_dispatch(CPU *cpu) {
    uint8_t cb_opcode = cpu_fetch(cpu);
    cpu->cycles = cb_opcode_cycles[cb_opcode];
    OpCodeHandler handler = cb_opcode_table[cb_opcode];
    if (handler) {
        handler(cpu);
    } else {
        //printf("UNKNOWN CB OPCODE : 0xCB%02X\n", cb_opcode);
        }
};
///////////////////////////////////////////////////
// FETCH DECODE/EXECUTE AND RUN FUNCTIONS HERE:
///////////////////////////////////////////////////
// fetch: 
uint8_t cpu_fetch(CPU *cpu) {
    return mmu_read8(cpu->mmu, cpu->PC++);  
}

//decode/execute: 
void execute(CPU *cpu, uint8_t opcode) {
    cpu->cycles = opcode_cycles[opcode];
    uint8_t old_z = GET_FLAG(cpu, FLAG_Z);

    if (opcode_table[opcode]) {
        opcode_table[opcode](cpu);
        if (GET_FLAG(cpu, FLAG_Z) != old_z) {
        }
    } else {
        //printf("UNKNOWN OPCODE : 0x%02X\n", opcode);
    }
};

//run: 
int frame_counter = 0;
void run(CPU *cpu, PPU *ppu) {
    int total_cycles = 0;
    
    while (true) {
        
        if (cpu->halted) {
            update_timers(cpu, 1);
            ppu_step(ppu, cpu->mmu, cpu->cycles * 4);
            uint8_t IE = mmu_read8(cpu->mmu, 0xFFFF);
            uint8_t IF = mmu_read8(cpu->mmu, 0xFF0F);
            if (IE & IF & 0x1F) cpu->halted = 0;
            else if(IE == 0) cpu->halted = 0;
            total_cycles += 4;
            continue;
        }
        //log_cpu_state(cpu);
        uint8_t opcode = cpu_fetch(cpu);
        execute(cpu, opcode);
        if (cpu->ime_pending) {
            cpu->ime_pending--;
            if(cpu->ime_pending == 0){
            cpu->IME = 1;
            }
        }
        update_timers(cpu, cpu->cycles);
        ppu_step(ppu, cpu->mmu, cpu->cycles * 4);
        check_interrupts(cpu);
        ppu_handle_events();
    }

    fclose(logfile);
}
void dump_vram_text(MMU *mmu) {
    for (int i = 0x9800; i <= 0x9BFF; i++) {
        char c = mmu_read8(mmu, i);
        if (c >= 32 && c <= 126) printf("%c", c);
        else printf(".");
    }
}
