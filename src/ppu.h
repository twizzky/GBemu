#ifndef PPU_H
#define PPU_H
#include <stdint.h>
#include "memory.h"

typedef struct PPU {
    MMU *mmu;                // to read/write PPU registers + VRAM
    uint8_t mode;            // 0=HBlank, 1=VBlank, 2=OAM scan, 3=drawing
    int cycles;
    uint8_t current_line;
} PPU;

void ppu_init(PPU *ppu);
void ppu_step(PPU *ppu, MMU *mmu, int cycles);
void ppu_render_scanline(PPU *ppu, MMU *mmu);
void ppu_render(void);
void ppu_handle_events(void);

#endif
