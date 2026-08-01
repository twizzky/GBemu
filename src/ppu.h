#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include "memory.h"

typedef struct PPU {
    MMU *mmu;                // to read/write PPU registers + VRAM
    uint8_t mode;            // 0=HBlank, 1=VBlank, 2=OAM scan, 3=drawing
    uint8_t line;            // LY register: current scanline 0-153
    uint16_t dots;           // dot position within the current line (0-455)
    uint8_t prev_lyc_match;  // previous LY==LYC state (edge detection)
} PPU;

void ppu_init(PPU *ppu, MMU *mmu);
void ppu_step(PPU *ppu, int dots);

void ppu_render(void);
void ppu_handle_events(void);

#endif
