#include "common.h"
#include "sound_assets.h"

extern "C" const uint8 MARS_SFX_DATA[];

// Audio is mixed on the master SH-2. DMA streams word-sized mono samples to
// both PWM outputs; the slave keeps its existing renderer and interrupt mask.
static const int32 SAMPLE_RATE = 11025;
static const int32 BLOCK_SIZE = 256;
static const int32 VOICES = 4;
static const int8 imaAdjust[8] = {-1,-1,-1,-1,2,4,6,8};

struct Channel {
    const uint8* data;
    uint32 length;
    uint32 position;
    uint32 decoded;
    int32 predictor;
    int32 stepIndex;
    int32 increment;
    int32 volume;
    int32 sampleId;
    uint32 age;
};

static Channel channels[VOICES];
static ALIGN16 volatile uint16 audioBuffers[2][BLOCK_SIZE];
static const MarsSoundLevel* soundLevel;
static int32 playBuffer;
static int32 period;
static bool initialized;
static uint32 nextAge;

volatile uint32 gMarsAudioBlocks = 0;
volatile uint32 gMarsAudioStarts = 0;
volatile uint32 gMarsAudioClips = 0;
volatile uint32 gMarsAudioPeakVoices = 0;

void marsSetLevelSounds()
{
    int32 index = gLevelID == LVL_TR1_GYM ? 0 : (gLevelID == LVL_TR1_2 ? 2 : 1);
    soundLevel = soundLevels + index;
    level.soundMap = soundLevel->map;
    level.soundsInfo = soundLevel->infos;
    level.soundOffsetsCount = soundLevel->count;
    level.soundData = MARS_SFX_DATA;
    // The backend resolves local IDs through sampleIds rather than PKD offsets.
    level.soundOffsets = NULL;
}

static void resetChannel(Channel &ch)
{
    ch.position = 0;
    ch.decoded = 0;
    ch.predictor = 0;
    ch.stepIndex = 0;
}

static X_INLINE void decodeNext(Channel &ch)
{
    uint32 packed = ch.data[ch.decoded >> 1];
    int32 code = (packed >> ((ch.decoded & 1) << 2)) & 15;
    // 1.4 KB of precomputed deltas removes three branches per decoded sample.
    int32 diff = imaDelta[ch.stepIndex][code & 7];
    ch.predictor = X_CLAMP(ch.predictor + ((code & 8) ? -diff : diff), -32768, 32767);
    ch.stepIndex = X_CLAMP(ch.stepIndex + imaAdjust[code & 7], 0, 88);
    ch.decoded++;
}

static void mixBlock(volatile uint16* out)
{
    int32 active = 0;
    for (int32 i = 0; i < VOICES; i++) if (channels[i].data) active++;
    if (uint32(active) > gMarsAudioPeakVoices) gMarsAudioPeakVoices = active;
    const int32 center = period >> 1;
    if (!active || !gSettings.audio_sfx) {
        fast_memset((void*)out, center | (center << 16), BLOCK_SIZE / 2);
        // Muting cancels active effects rather than resuming stale sounds later.
        if (!gSettings.audio_sfx) for (int32 i = 0; i < VOICES; i++) channels[i].data = NULL;
        return;
    }
    for (int32 n = 0; n < BLOCK_SIZE; n++)
    {
        int32 mixed = 0;
        for (int32 i = 0; i < VOICES; i++)
        {
            Channel &ch = channels[i];
            if (!ch.data) continue;
            uint32 wanted = ch.position >> 8;
            while (ch.decoded <= wanted)
            {
                decodeNext(ch);
            }
            mixed += (ch.predictor * ch.volume) >> SND_VOL_SHIFT;
            ch.position += ch.increment;
            if ((ch.position >> 8) >= ch.length) ch.data = NULL;
        }
        mixed >>= 1; // 6 dB of headroom for overlapping effects.
        if (mixed < -32768 || mixed > 32767) gMarsAudioClips++;
        mixed = X_CLAMP(mixed, -32768, 32767);
        out[n] = center + ((mixed * (center - 1)) >> 15);
    }
}

static void startDMA()
{
    SH2_DMA_SAR1 = uint32(audioBuffers[playBuffer]);
    SH2_DMA_TCR1 = BLOCK_SIZE;
    SH2_DMA_CHCR1 = 0x14E5; // word, increment source, fixed destination, external request + IRQ
}

extern "C" void pri_dma1_handler()
{
    uint32 status = SH2_DMA_CHCR1;
    if (!(status & 2)) return;
    SH2_DMA_CHCR1 = 0;
    playBuffer ^= 1;
    startDMA(); // Restart the prepared half before mixing the half just consumed.
    mixBlock(audioBuffers[playBuffer ^ 1]);
    gMarsAudioBlocks++;
}

void sndInit()
{
#ifdef MARS_NO_AUDIO
    gSettings.audio_sfx = 0;
    return;
#else
    int32 mask = SetSH2SR(15);
    period = (((MARS_VDP_DISPMODE & MARS_NTSC_FORMAT) ? 23011361 : 22801467) + SAMPLE_RATE/2) / SAMPLE_RATE;
    MARS_PWM_CTRL = 0;
    MARS_PWM_CYCLE = period + 1;
    MARS_PWM_MONO = period / 2;
    MARS_PWM_MONO = period / 2;
    MARS_PWM_MONO = period / 2;
    mixBlock(audioBuffers[0]);
    mixBlock(audioBuffers[1]);
    SH2_DMA_CHCR1 = 0;
    SH2_DMA_DAR1 = uint32(&MARS_PWM_MONO);
    SH2_DMA_DRCR1 = 0;
    SH2_DMA_VCR1 = 66;
    SH2_INT_IPRA = (SH2_INT_IPRA & 0xF0FF) | 0x0400;
    SH2_DMA_DMAOR = 1;
    playBuffer = 0;
    initialized = true;
    MARS_PWM_CTRL = 0x0185; // One DMA request per PWM cycle; route left/right normally.
    startDMA();
    SetSH2SR(mask);
#endif
}

void sndInitSamples() { }
void sndFreeSamples() { sndStop(); }

void* sndPlaySample(int32 index, int32 volume, int32 pitch, int32 mode)
{
    if (!initialized || !gSettings.audio_sfx || !soundLevel || index < 0 || uint32(index) >= soundLevel->count || volume <= 0) return NULL;
    int32 id = soundLevel->ids[index];
    int32 mask = SetSH2SR(15);
    Channel* target = NULL;
    if (mode == UNIQUE || mode == REPLAY)
    {
        for (int32 i = 0; i < VOICES; i++) if (channels[i].data && channels[i].sampleId == id) {
            target = channels + i;
            break;
        }
    }
    bool fresh = !target;
    if (!target) {
        for (int32 i = 0; i < VOICES; i++) if (!channels[i].data) { target = channels + i; break; }
    }
    if (!target) {
        target = channels;
        for (int32 i = 1; i < VOICES; i++) {
            if (channels[i].volume < target->volume ||
                (channels[i].volume == target->volume && channels[i].age < target->age)) target = channels + i;
        }
    }
    if (fresh || mode == REPLAY) {
        resetChannel(*target);
        target->data = MARS_SFX_DATA + sampleAssets[id].offset;
        target->length = sampleAssets[id].length;
        target->sampleId = id;
        target->age = nextAge++;
        gMarsAudioStarts++;
    }
    target->volume = X_CLAMP(volume, 1, 64);
    target->increment = X_CLAMP(pitch, 32, 512) * 2;
    SetSH2SR(mask);
    return target;
}

void sndStopSample(int32 index)
{
    if (!soundLevel || index < 0 || uint32(index) >= soundLevel->count) return;
    int32 id = soundLevel->ids[index];
    int32 mask = SetSH2SR(15);
    for (int32 i = 0; i < VOICES; i++) if (channels[i].sampleId == id) channels[i].data = NULL;
    SetSH2SR(mask);
}

void sndStop()
{
    int32 mask = SetSH2SR(15);
    for (int32 i = 0; i < VOICES; i++) channels[i].data = NULL;
    SetSH2SR(mask);
}

// The repository contains no packed music bank. SFX work without Sega CD;
// optional CD music must never gate initialization or gameplay.
void sndPlayTrack(int32 track) { (void)track; }
void sndStopTrack() { }
bool sndTrackIsPlaying() { return false; }
void sndFill(int8* buffer) { (void)buffer; } // PWM DMA owns the output buffers.

#ifdef MARS_RASTER_SELFTEST
volatile uint32 gMarsAudioTestHash = 0;
volatile uint32 gMarsAudioTestSamples = 0;
volatile uint32 gMarsAudioTestDone = 0;
void marsAudioSelfTest()
{
    uint32 hash = 2166136261u;
    uint32 count = 0;
    for (uint32 i = 0; i < sizeof(sampleAssets)/sizeof(sampleAssets[0]); i++) {
        Channel ch = {};
        ch.data = MARS_SFX_DATA + sampleAssets[i].offset;
        for (uint32 n = 0; n < sampleAssets[i].length; n++) {
            decodeNext(ch);
            hash = (hash ^ uint16(ch.predictor)) * 16777619u;
            count++;
        }
    }
    gMarsAudioTestHash = hash;
    gMarsAudioTestSamples = count;
    gMarsAudioTestDone = 1;
}
#endif
