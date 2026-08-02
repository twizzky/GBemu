#include "memory.h"
#include "cpu.h"
#include "ppu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
void mmu_init(MMU *mmu) {
    memset(mmu->memory, 0, MEMORY_SIZE);
    mmu->memory[0xFFFF] = 0x00; 
    mmu->memory[0xFF0F] = 0xE0;
    mmu->memory[0xFF40] = 0x91; // LCD on, BG enabled (no boot ROM runs, so pre-init it)
    mmu->memory[0xFF04] = 0x00;
    mmu->memory[0xFF05] = 0x00;
    mmu->memory[0xFF06] = 0x00;
    mmu->memory[0xFF07] = 0x00;
    mmu->rom_bank = 1;
    mmu->rom_data = NULL;
    mmu->rom_size = 0;
}

uint8_t mmu_read8(MMU *mmu, uint16_t address) {
    if (address == 0xFF04) return (mmu->cpu->internal_div >> 8) & 0xFF;
    if (mmu->ppu) {
        if(address == 0xFF00) return 0xFF;
        if (address == 0xFF44) return mmu->ppu->current_line;        // LY: live PPU state
        if (address == 0xFF41) {                                          // STAT
            uint8_t stat = mmu->memory[0xFF41];
            uint8_t lyc = (mmu->ppu->current_line == mmu->memory[0xFF45]) ? 0x04 : 0x00;
            return (stat & 0x78) | lyc | (mmu->ppu->mode & 0x03);
        }
    }
    if (address >= 0xFEA0 && address <= 0xFEFF) return 0xFF;
    if (address == 0xFF0F) return mmu->memory[0xFF0F] | 0xE0;
    if (address >= 0x4000 && address <= 0x7FFF)
        return mmu->rom_data[address - 0x4000 + mmu->rom_bank * 0x4000];
    return mmu->memory[address];
}
void mmu_write8(MMU *mmu, uint16_t address, uint8_t value) {
    if (address == 0xFF0F) { mmu->memory[0xFF0F] = value | 0xE0; return; }
    if (address == 0xFF04) { mmu->cpu->internal_div = 0; return; }
    if (address == 0xFFFF) { mmu->memory[0xFFFF] = value; return; }
    if (address == 0xFF44) return;  // LY is read-only on real hardware
    if (address == 0xFF41) {        // STAT: only bits 3-6 are writable
        mmu->memory[0xFF41] = (mmu->memory[0xFF41] & 0x07) | (value & 0x78);
        return;
    }
    if (address == 0xFF40 && mmu->ppu) {   // LCDC
        uint8_t old = mmu->memory[0xFF40];
        mmu->memory[0xFF40] = value;
        // Turning the LCD off resets the PPU state machine.
        if ((old & 0x80) && !(value & 0x80)) {
            mmu->ppu->current_line = 0;
            mmu->ppu->cycles = 0;
            mmu->ppu->mode = 0;
        }
        return;
    }
    if (address == 0xFF07) {
        uint8_t old_tac = mmu->memory[0xFF07];
        mmu->memory[0xFF07] = value;
        if ((old_tac & 0x04) != (value & 0x04)) {
            mmu->cpu->timer_counter = 0;
        }
        return;
    }
    if (address == 0xFF05) {
        mmu->memory[0xFF05] = value;
        mmu->cpu->timer_counter = 0;
        return;
    }
    if (address == 0xFF02 && value == 0x81) {
        printf("%c", mmu_read8(mmu, 0xFF01));
        fflush(stdout);
        mmu->memory[0xFF02] = 0x00;
        return;
    }
    if (address >= 0x2000 && address <= 0x3FFF) {
        uint8_t bank = value & 0x1F;
        if (bank == 0) bank = 1;
        mmu->rom_bank = bank;
        return;
    }
    if (address < 0x8000) return;
    if (address >= 0xFEA0 && address <= 0xFEFF) return;
    mmu->memory[address] = value;
}

bool mmu_load_rom(MMU *mmu, const char *filename) {
    FILE *rom = fopen(filename, "rb");
    if (!rom) {
        fprintf(stderr, "Failed to open ROM file: %s\n", filename);
        return false;
    }

    // get file size
    fseek(rom, 0, SEEK_END);
    mmu->rom_size = ftell(rom);
    rewind(rom);

    // allocate and load full ROM
    mmu->rom_data = malloc(mmu->rom_size);
    if (!mmu->rom_data) {
        fprintf(stderr, "Failed to allocate ROM buffer\n");
        fclose(rom);
        return false;
    }

    size_t bytes_read = fread(mmu->rom_data, 1, mmu->rom_size, rom);
    fclose(rom);

    if (bytes_read != mmu->rom_size) {
        fprintf(stderr, "Failed to read ROM file: %s\n", filename);
        return false;
    }

    // copy bank 0 into memory as before so everything still works
    memcpy(mmu->memory, mmu->rom_data, 0x8000 < mmu->rom_size ? 0x8000 : mmu->rom_size);

    printf("ROM loaded successfully.\n");
    return true;
}
void mmu_write16(MMU *mmu, uint16_t addr, uint16_t val) {
    mmu_write8(mmu, addr, val & 0xFF);
    mmu_write8(mmu, addr + 1, (val >> 8) & 0xFF);
}

uint16_t mmu_read16(MMU *mmu, uint16_t addr) {
    
    uint8_t lo = mmu_read8(mmu, addr);
    uint8_t hi = mmu_read8(mmu, addr + 1);
    return lo | (hi << 8);
}


