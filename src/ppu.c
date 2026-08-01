#include <SDL2/SDL.h>
#include "ppu.h"
#include "memory.h"

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144
#define SCALE 3  // 160x144 scaled up to 480x432

// ------------------------------------------------
// PPU timing constants
// ------------------------------------------------
#define DOTS_PER_LINE     456   // every scanline takes exactly 456 dots
#define TOTAL_LINES       154   // 144 visible + 10 VBlank
#define VBLANK_START      144   // LY value where VBlank begins
#define MODE2_DOTS        80    // OAM scan
#define MODE3_DOTS        172   // pixel drawing (fixed approximation)
// mode 0 (HBlank) fills the rest: 456 - 80 - 172 = 204 dots

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static uint32_t framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT];

void ppu_init(PPU *ppu, MMU *mmu) {
    ppu->mmu = mmu;
    ppu->mode = 0;
    ppu->line = 0;
    ppu->dots = 0;
    ppu->prev_lyc_match = 0;

    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow(
        "GBemu",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH * SCALE,
        SCREEN_HEIGHT * SCALE,
        0
    );
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );
}

// Advance the PPU by `dots` PPU clock dots (called after each CPU instruction).
void ppu_step(PPU *ppu, int dots) {
    MMU *mmu = ppu->mmu;
    uint8_t lcdc = mmu_read8(mmu, 0xFF40);

    // LCD disabled: PPU frozen at line 0, mode 0, no interrupts.
    if (!(lcdc & 0x80)) return;

    ppu->dots += dots;

    // Cross any line boundaries. A single CPU instruction can span lines.
    while (ppu->dots >= DOTS_PER_LINE) {
        ppu->dots -= DOTS_PER_LINE;
        ppu->line++;

        if (ppu->line == VBLANK_START) {
            // Entering VBlank: request the VBlank interrupt (IF bit 0).
            mmu_write8(mmu, 0xFF0F, mmu_read8(mmu, 0xFF0F) | 0x01);
            // STAT mode-1 interrupt, if enabled (STAT bit 4).
            if (mmu_read8(mmu, 0xFF41) & 0x10)
                mmu_write8(mmu, 0xFF0F, mmu_read8(mmu, 0xFF0F) | 0x02);
            ppu->mode = 1;
        }
        if (ppu->line >= TOTAL_LINES) {
            ppu->line = 0; // frame complete, back to scanline 0
        }
    }

    // Where are we inside the current line?
    uint8_t new_mode;
    if (ppu->line >= VBLANK_START) {
        new_mode = 1;
    } else if (ppu->dots < MODE2_DOTS) {
        new_mode = 2;
    } else if (ppu->dots < MODE2_DOTS + MODE3_DOTS) {
        new_mode = 3;
    } else {
        new_mode = 0;
    }

    // STAT interrupts are edge-triggered: fire only on mode transitions,
    // and only if the matching enable bit (STAT bit 3/4/5) is set.
    if (new_mode != ppu->mode) {
        uint8_t stat = mmu_read8(mmu, 0xFF41);
        uint8_t bit = (new_mode == 0) ? 0x08 : (new_mode == 1) ? 0x10
                                                               : (new_mode == 2) ? 0x20 : 0x00;
        if (bit && (stat & bit))
            mmu_write8(mmu, 0xFF0F, mmu_read8(mmu, 0xFF0F) | 0x02);
        ppu->mode = new_mode;
    }

    // LYC == LY interrupt (STAT bit 6), edge-triggered on match.
    uint8_t lyc_match = (ppu->line == mmu_read8(mmu, 0xFF45)) ? 1 : 0;
    if (lyc_match && !ppu->prev_lyc_match && (mmu_read8(mmu, 0xFF41) & 0x40))
        mmu_write8(mmu, 0xFF0F, mmu_read8(mmu, 0xFF0F) | 0x02);
    ppu->prev_lyc_match = lyc_match;
}

void ppu_render(void) {
    SDL_UpdateTexture(texture, NULL, framebuffer, SCREEN_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void ppu_handle_events(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) exit(0);
    }
}
