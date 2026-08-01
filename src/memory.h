#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#define MEMORY_SIZE 65536

typedef struct CPU CPU;
typedef struct PPU PPU;
typedef struct MMU {
    uint8_t memory[MEMORY_SIZE];
    CPU *cpu;
    PPU *ppu;               // PPU state, used for LY/STAT reads
    uint8_t *rom_data;      // raw ROM bytes
    uint32_t rom_size;      // total ROM size in bytes
    uint8_t rom_bank;       // current ROM bank (default 1)
} MMU;
void mmu_init(MMU *mmu);
uint8_t mmu_read8(MMU *mmu, uint16_t address);
void mmu_write8(MMU *mmu, uint16_t address, uint8_t value);
bool mmu_load_rom(MMU *mmu, const char *filename);
void mmu_write16(MMU *mmu, uint16_t addr, uint16_t val);
uint16_t mmu_read16(MMU *mmu, uint16_t addr);

#endif
