#include "../worker.h"
#define REG8(a) (*(volatile uint8_t*)(a))
#define REG16(a) (*(volatile uint16_t*)(a))
extern void mwDecodeFast(const MWVoice*,MWVoiceResult*);

// A private cartridge-boot worker. It calls no CD BIOS or drive functions and
// touches only PRG/Word RAM and communication registers, never backup RAM.
void cd_worker_main(void)
{
#ifdef MARS_CD_STALL_TEST
    uint32_t jobs = 0;
#endif
    REG8(0xFF8003) = 1; // Initial Word RAM ownership belongs to the main CPU.
    REG16(0xFF8020) = REG16(0xFF8010); // Fresh boot acknowledgement, also after VRES.
    REG8(0xFF800F) = 0xA5;
    for (;;) {
        if (REG8(0xFF800E) != 1 || (REG8(0xFF8003) & 1)) continue;
#ifdef MARS_CD_STALL_TEST
        if (++jobs == 4) for (;;) { }
#endif
        const MWVoice* input = (const MWVoice*)(0x080000 + MW_CD_INPUT);
        MWVoiceResult* output = (MWVoiceResult*)(0x080000 + MW_CD_OUTPUT);
        MWVoice mapped = *input;
        mapped.rom = 0x080000 + MW_CD_BYTES - (input->decoded >> 1);
        if (input->active && input->step <= 88 && input->increment <= 1024 &&
            input->increment >= 32) mwDecodeFast(&mapped, output);
        // Return all 2M Word RAM before announcing completion.
        REG8(0xFF8003) = 1;
        REG8(0xFF800F) = 0x5A;
        while (REG8(0xFF800E) != 0) { }
        REG8(0xFF800F) = 0xA5;
    }
}
