// Shared exact IMA decoder/resampler for the Genesis and CD 68000s.
// MW_READ_BYTE(address) and MW_CODE select each processor's memory interface.
#include "../worker.h"
#include "worker_tables.h"
static MW_CODE void mwDecode(const MWVoice* input, MWVoiceResult* result)
{
    uint32_t position = input->position, decoded = input->decoded;
    int32_t predictor = input->predictor;
    uint16_t step = input->step;
    unsigned n;
    for (n = 0; n < MW_SAMPLES; n++) {
        if (!input->active || (position >> 8) >= input->length) {
            result->pcm[n] = 0;
            continue;
        }
        while (decoded <= (position >> 8)) {
            uint16_t code = MW_READ_BYTE(input->rom + (decoded >> 1));
            code = (code >> ((decoded & 1) << 2)) & 15;
            int32_t diff = mwDelta[step][code & 7];
            predictor += (code & 8) ? -diff : diff;
            if (predictor < -32768) predictor = -32768;
            if (predictor > 32767) predictor = 32767;
            step = mwNext[step][code & 7];
            decoded++;
        }
        result->pcm[n] = predictor;
        position += input->increment;
    }
    result->position = position;
    result->decoded = decoded;
    result->predictor = predictor;
    result->step = step;
}
