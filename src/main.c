#include "cpu.h"
#include "memory.h" 
#include "ppu.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
int main() {
    CPU cpu;
    MMU mmu;
    PPU ppu;
    mmu_init(&mmu);
    initCPU(&cpu, &mmu); 

    if (!mmu_load_rom(&mmu,"./test/cpu_instrs.gb")) {
        printf("FAILED.");
        return 1;
    }
    printf("ROM loaded successfully.\n");

    ppu_init(&ppu, &mmu);
    mmu.ppu = &ppu;
    run(&cpu);
    return 0;
}