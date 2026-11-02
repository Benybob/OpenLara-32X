#include "../worker.h"
#define REG8(a) (*(volatile uint8_t*)(a))
#define REG16(a) (*(volatile uint16_t*)(a))
#define MW_CODE __attribute__((section(".ramtext")))
#define MW_TABLE __attribute__((section(".data.worker_tables")))
extern const uint8_t cd_worker_blob[], cd_worker_blob_end[];
volatile uint32_t mdWorkerTicks;
volatile uint32_t mdWorkerJobs, mdWorkerCDJobs, mdWorkerTimeouts;
static MWJob job;
static MWResult result;
static uint16_t received;
static int cdState;
static uint32_t romBank;

static MW_CODE uint8_t romByte(uint32_t address)
{
    uint32_t bank = address >> 20;
    if (bank != romBank) { REG16(0xA15104) = bank; romBank = bank; }
    return REG8(0x900000 + (address & 0xFFFFF));
}
#define MW_READ_BYTE(address) romByte(address)
#include "worker_decode.h"
extern void mwDecodeFast(const MWVoice*,MWVoiceResult*);
static MW_CODE void decodeVoice(const MWVoice* input,MWVoiceResult* output)
{
    uint32_t address = input->rom + (input->decoded >> 1);
    if ((address & 0xFFFFF) > 0x100000-514) { mwDecode(input,output); return; }
    MWVoice mapped = *input;
    romBank = address >> 20;
    REG16(0xA15104) = romBank;
    mapped.rom = 0x900000 + (address & 0xFFFFF) - (input->decoded >> 1);
    mwDecodeFast(&mapped,output);
}

static MW_CODE int expired(uint32_t start)
{
    return (uint32_t)(mdWorkerTicks - start) >= 3;
}
static MW_CODE int cdPresent(void)
{
    // Existing Mode-1 BIOS signatures. Never probe/write CD registers unless
    // a supported BIOS signature was found in the cartridge-side ROM window.
    static const uint32_t locations[] MW_TABLE = {0x400100,0x41586D,0x41606D,0x41AD6D};
    unsigned i;
    for (i = 0; i < 4; i++) {
        uint32_t a = locations[i];
        if (REG8(a)=='S' && REG8(a+1)=='E' && REG8(a+2)=='G' && REG8(a+3)=='A') return 1;
        if (REG8(a)=='W' && REG8(a+1)=='O' && REG8(a+2)=='N' && REG8(a+3)=='D' &&
            REG8(a+4)=='E' && REG8(a+5)=='R') return 1;
    }
    return 0;
}
static MW_CODE int cdInit(void)
{
    uint32_t start, n;
    uint16_t token;
    if (cdState) return cdState > 0;
    cdState = -1;
    if (!cdPresent()) return 0;
    REG8(0xA12001) = 2; // Reset sub-CPU and acquire PRG-RAM.
    start = mdWorkerTicks;
    while (!(REG8(0xA12001) & 2)) if (expired(start)) return 0;
    REG16(0xA12002) = 0; // No PRG write protection, bank zero, 2M Word RAM.
    for (n = 0; n < (uint32_t)(cd_worker_blob_end-cd_worker_blob); n++)
        REG8(0x420000+n) = cd_worker_blob[n];
    token = REG16(0xA12020) + 1;
    REG16(0xA12010) = token;
    REG8(0xA1200E) = 0;
    REG8(0xA12001) = 1; // Execute the private reset vector, without drive I/O.
    start = mdWorkerTicks;
    while (REG8(0xA1200F) != 0xA5 || REG16(0xA12020) != token)
        if (expired(start)) return 0;
    cdState = 1;
    return 1;
}
static MW_CODE int cdStart(const MWVoice* voice)
{
    uint32_t first, last, n, start = mdWorkerTicks;
    if (!cdInit()) return 0;
    while (REG8(0xA1200F) != 0xA5 || !(REG8(0xA12003)&1)) {
        if (expired(start)) { cdState = -1; return 0; }
    }
    volatile uint16_t* dst = (volatile uint16_t*)0x600000;
    const uint16_t* src = (const uint16_t*)voice;
    for (n = 0; n < sizeof(*voice)/2; n++) dst[n] = src[n];
    first = voice->decoded >> 1;
    last = ((voice->position + (MW_SAMPLES-1)*voice->increment) >> 8) + 1;
    if (last > voice->length) last = voice->length;
    last = (last + 1) >> 1;
    if (last < first || last-first > 513) return 0;
    for (n = first; n < last; n++) REG8(0x600000+MW_CD_BYTES+n-first) = romByte(voice->rom+n);
    REG8(0xA1200E) = 1;
    REG8(0xA12003) = 2; // DMNA: pass all Word RAM to the CD CPU.
    return 1;
}
static MW_CODE int cdFinish(MWVoiceResult* output)
{
    uint32_t start = mdWorkerTicks, n;
    while (REG8(0xA1200F) != 0x5A || !(REG8(0xA12003)&1)) {
        if (expired(start)) {
            REG8(0xA1200E) = 0; cdState = -1; mdWorkerTimeouts++; return 0;
        }
    }
    volatile uint16_t* src = (volatile uint16_t*)(0x600000+MW_CD_OUTPUT);
    uint16_t* dst = (uint16_t*)output;
    for (n = 0; n < sizeof(*output)/2; n++) dst[n] = src[n];
    REG8(0xA1200E) = 0;
    mdWorkerCDJobs++;
    return 1;
}
static MW_CODE void execute(void)
{
    unsigned i, cdSlot = MW_VOICES;
    uint32_t start;
#ifdef MARS_MD_STALL_TEST
    if (mdWorkerJobs == 3) for (;;) { }
#endif
    result.magic = MW_MAGIC; result.sequence = job.sequence;
    result.mask = 0; result.backend = 1;
    romBank = 0xFFFFFFFF;
    for (i = 0; i < MW_VOICES; i++) {
        MWVoice* v = job.voices+i;
        if (!v->active) continue;
        if (v->step > 88 || v->increment < 32 || v->increment > 1024 ||
            v->rom >= 0x400000 || v->length > 0x100000 || !v->length ||
            v->rom + ((v->length+1)>>1) > 0x400000 ||
            v->decoded > (v->position>>8) + 1) { v->active = 0; continue; }
        result.mask |= 1 << i;
        if ((job.mode & 0xFF) == 2 && cdSlot == MW_VOICES && cdStart(v)) cdSlot = i;
    }
    for (i = 0; i < MW_VOICES; i++)
        if ((result.mask & (1<<i)) && i != cdSlot) decodeVoice(job.voices+i, result.voices+i);
    if (cdSlot < MW_VOICES) {
        if (cdFinish(result.voices+cdSlot)) result.backend = 3;
        else decodeVoice(job.voices+cdSlot,result.voices+cdSlot);
    }
    // The SH2 arms DMA0 before posting the descriptor. Four-word FIFO writes
    // preserve the hardware's transfer granularity; no per-sample handshakes.
    REG8(0xA15107) = 0;
    unsigned words = (8 + (job.mode>>8)*sizeof(MWVoiceResult) + 7) / 8 * 4;
    REG16(0xA15110) = words;
    REG8(0xA15107) = 4;
    const uint16_t* src = (const uint16_t*)&result;
    start = mdWorkerTicks;
    for (i = 0; i < words; i += 4) {
        while (REG8(0xA15107)&0x80) {
            if (expired(start)) { REG8(0xA15107)=0; mdWorkerTimeouts++; return; }
        }
        REG16(0xA15112)=src[i]; REG16(0xA15112)=src[i+1];
        REG16(0xA15112)=src[i+2]; REG16(0xA15112)=src[i+3];
    }
    mdWorkerJobs++;
}

MW_CODE void md_worker_request(void)
{
    uint16_t index = (REG16(0xA15120)-MW_COMMAND)*3;
    if (!index) received = 0;
    if (index == received && index < sizeof(job)/2) {
        uint16_t* dst = (uint16_t*)&job;
        dst[index] = REG16(0xA15122);
        if (index+1 < sizeof(job)/2) dst[index+1] = REG16(0xA1512C);
        if (index+2 < sizeof(job)/2) dst[index+2] = REG16(0xA1512E);
        received += 3;
    } else received = 0;
    REG16(0xA15120) = 0; // Acknowledge descriptor transport before computing.
    unsigned count = job.mode >> 8;
    if (count >= 1 && count <= MW_VOICES && received >= 2+count*sizeof(MWVoice)/2) {
        unsigned i;
        for (i = count; i < MW_VOICES; i++) job.voices[i].active = 0;
        received = 0; execute();
    }
}
