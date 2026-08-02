#include <SDL2/SDL.h>
#include <stdint.h>
#include <string.h>
#include "ppu.h"
#include "memory.h"

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144
#define SCALE 3  // 160x144 scaled up to 480x432

static uint32_t palette[4] = {
    0xFFE0F8D0, // White
    0xFF88C070, // Light gray
    0xFF346856, // Dark gray
    0xFF081820  // Black
};

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static uint32_t framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT];

void ppu_init(PPU *ppu){
    ppu->mode = 0;
    ppu->cycles = 0;
    ppu->current_line = 0;

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
    memset(framebuffer, 0, sizeof(framebuffer));
}

void ppu_render_scanline(PPU *ppu, MMU *mmu){
    uint8_t lcdc = mmu_read8(mmu, 0xFF40);
    if(!(lcdc & 0x80)){
        for( int x = 0; x < SCREEN_WIDTH; x++){
            framebuffer[ppu->current_line * SCREEN_WIDTH + x] = palette[0];
        }
        return;
    }
    uint16_t tile_map = (lcdc & 0x08) ? 0x9C00 : 0x9800;
    uint16_t tile_data = (lcdc & 0x10) ? 0x8000 : 0x8800;
    uint8_t signed_tile = (tile_data == 0x8800);

    uint8_t scx = mmu->memory[0xFF43];
    uint8_t scy = mmu->memory[0xFF42];

    uint8_t y = ppu->current_line + scy;
    uint16_t tile_row =  (y/8) * 32;
    for(int x = 0; x < SCREEN_WIDTH; x++){
        uint8_t px = x + scx;
        uint16_t tile_col = px / 8;
        uint16_t tile_addr = tile_map + tile_row + tile_col;
        int16_t tile_num;
        if (signed_tile){
            tile_num = (int8_t)mmu->memory[tile_addr];
        }
        else{
            tile_num = mmu->memory[tile_addr];
        }
        uint16_t tile_location;
        if(signed_tile){
            tile_location = tile_data + (tile_num + 128) * 16;
        }
        else {
            tile_location = tile_data + tile_num * 16;
        }
        uint8_t tile_y = y % 8;
        uint8_t tile_byte1 = mmu->memory[tile_location + tile_y * 2];
        uint8_t tile_byte2 = mmu->memory[tile_location + tile_y * 2 + 1];
        
        uint8_t tile_x = 7 - (px % 8);
        uint8_t color_lo = (tile_byte1 >> tile_x) & 1;
        uint8_t color_hi = (tile_byte2 >> tile_x) & 1;
        uint8_t color_id = (color_hi << 1) | color_lo;

        uint8_t bgp = mmu->memory[0xFF47];
        uint8_t color = (bgp >> (color_id * 2)) & 0x03;

        framebuffer[ppu->current_line * SCREEN_WIDTH + x] = palette[color];
    }
}

void ppu_step(PPU *ppu, MMU *mmu, int cycles){
    ppu->cycles += cycles;

    switch (ppu->mode){
        case 2:
            if (ppu->cycles >= 80){
                ppu->cycles -= 80;
                ppu->mode = 3;
            }
            break;
        case 3:
            if (ppu->cycles >= 172){
                ppu->cycles -= 172;
                ppu->mode = 0;
                ppu_render_scanline(ppu, mmu);
            }
            break;
        case 0:
            if (ppu->cycles >= 204){
                ppu->cycles -= 204;
                ppu->current_line++;
                mmu->memory[0xFF44] = ppu->current_line;
                if(ppu->current_line == 144){
                    ppu->mode = 1;
                    // Trigger VBlank interrupt
                    mmu->memory[0xFF0F] |= 0x01;
                    ppu_render();
                }
                else{
                    ppu->mode = 2;
                }
            }
            break;
        case 1:
            if(ppu->cycles >= 456){
                ppu->cycles -= 456;
                ppu->current_line++;
                mmu->memory[0xFF44] = ppu->current_line;
                if(ppu->current_line > 153){
                    ppu->current_line = 0;
                    mmu->memory[0xFF44] = ppu->current_line;
                    ppu->mode = 2;
                }
            }
            break;
    }
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
