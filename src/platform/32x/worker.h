#ifndef MARS_WORKER_H
#define MARS_WORKER_H
#include <stdint.h>

// Big endian on all three processors. No pointers cross a processor boundary.
#define MW_SAMPLES 256
#define MW_VOICES 2
#define MW_COMMAND 0x0600
#define MW_MAGIC 0x574B
#define MW_CD_INPUT 0x0000
#define MW_CD_OUTPUT 0x0800
#define MW_CD_BYTES 0x1000
#define MW_CD_CODE 0x0200

typedef struct {
    uint32_t rom, length, position, decoded;
    int16_t predictor;
    uint16_t step, increment, active;
} MWVoice;
typedef struct {
    uint16_t sequence, mode;
    MWVoice voices[MW_VOICES];
} MWJob;
typedef struct {
    uint32_t position, decoded;
    int16_t predictor;
    uint16_t step;
    int16_t pcm[MW_SAMPLES];
} MWVoiceResult;
typedef struct {
    uint16_t magic, sequence, mask, backend;
    MWVoiceResult voices[MW_VOICES];
} MWResult;

#endif
