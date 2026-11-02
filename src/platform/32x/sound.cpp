#include "common.h"
#include "sound_assets.h"
#include "worker.h"

extern "C" const uint8 MARS_SFX_DATA[];
extern int32 gFrameIndex;

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

#ifndef MARS_WORKER_MODE
#define MARS_WORKER_MODE 1
#endif
#ifndef MARS_WORKER_VOICES
#define MARS_WORKER_VOICES 2
#endif
#if MARS_WORKER_VOICES < 1 || MARS_WORKER_VOICES > MW_VOICES
#error MARS_WORKER_VOICES must be 1 or 2
#endif
static_assert(sizeof(MWVoice) == 24 && sizeof(MWVoiceResult) == 524 &&
    sizeof(MWResult) == 1056, "Worker wire layout changed");
volatile uint32 gMarsWorkerMode = MARS_WORKER_MODE;
volatile uint32 gMarsWorkerJobs, gMarsWorkerUsed, gMarsWorkerStale;
volatile uint32 gMarsWorkerLate, gMarsWorkerFaults, gMarsWorkerBackend;
volatile uint32 gMarsWorkerVerifyErrors;
volatile uint32 gMarsWorkerCDJobs;
static ALIGN16 MWResult workerStorage;
static MWJob workerJob;
static int32 workerChannels[MW_VOICES];
static bool workerBusy;
static uint32 workerFrame;
static uint16 workerSequence;
static uint32 workerNow() { return *(volatile uint32*)&gFrameIndex; }

static volatile MWResult* workerResult()
{
    return &workerStorage;
}

void marsCycleAudioWorker()
{
    int32 mask = SetSH2SR(15);
    gMarsWorkerMode = (gMarsWorkerMode + 1) % 3;
    SetSH2SR(mask);
}

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

static __attribute__((section(".ramtext.audio"))) uint32 collectWorker()
{
    if (!workerBusy) return 0;
    if (!(SH2_DMA_CHCR0 & 2)) {
        gMarsWorkerLate++;
        if (workerNow() - workerFrame > 10) {
            SH2_DMA_CHCR0 = 0;
            gMarsWorkerMode = 0;
            gMarsWorkerFaults++;
            workerBusy = false;
        }
        return 0;
    }
    SH2_DMA_CHCR0 = 0;
    workerBusy = false;
    for (uint32 n = 0; n < sizeof(workerStorage); n += 16)
        CacheClearLine((uint8*)&workerStorage+n);
    volatile MWResult* r = workerResult();
    if (r->magic != MW_MAGIC || r->sequence != workerJob.sequence) {
        gMarsWorkerFaults++; gMarsWorkerMode = 0; return 0;
    }
    gMarsWorkerJobs++;
    gMarsWorkerBackend = r->backend;
    if (r->backend & 2) gMarsWorkerCDJobs++;
    uint32 valid = 0;
    for (int32 i = 0; i < MW_VOICES; i++) {
        if (!(r->mask & (1 << i))) continue;
        Channel &ch = channels[workerChannels[i]];
        const MWVoice &v = workerJob.voices[i];
        if (ch.data && uint32(ch.data)-0x02000000 == v.rom &&
            ch.length == v.length && ch.position == v.position &&
            ch.decoded == v.decoded && ch.predictor == v.predictor &&
            ch.stepIndex == v.step && ch.increment == v.increment &&
            r->voices[i].step <= 88) {
#ifdef MARS_WORKER_VERIFY
            Channel ref = ch;
            for (int32 n = 0; n < MW_SAMPLES; n++) {
                int32 value = 0;
                if ((ref.position >> 8) < ref.length) {
                    while (ref.decoded <= (ref.position >> 8)) decodeNext(ref);
                    value = ref.predictor;
                    ref.position += ref.increment;
                }
                if (value != r->voices[i].pcm[n]) gMarsWorkerVerifyErrors++;
            }
            if (ref.position != r->voices[i].position || ref.decoded != r->voices[i].decoded ||
                ref.predictor != r->voices[i].predictor || ref.stepIndex != r->voices[i].step)
                gMarsWorkerVerifyErrors++;
#endif


            valid |= 1 << i;
            gMarsWorkerUsed++;
        } else gMarsWorkerStale++;
    }
    return valid;
}

static __attribute__((section(".ramtext.audio"))) void queueWorker()
{
    if (!gMarsWorkerMode || workerBusy || MARS_SYS_COMM0) return;
    workerJob.sequence = ++workerSequence;
    workerJob.mode = gMarsWorkerMode;
    int32 count = 0;
    for (int32 i = 0; i < VOICES && count < MARS_WORKER_VOICES; i++) {
        const Channel &ch = channels[i];
        if (!ch.data) continue;
        MWVoice &v = workerJob.voices[count];
        v.rom = uint32(ch.data) - 0x02000000;
        v.length = ch.length; v.position = ch.position; v.decoded = ch.decoded;
        v.predictor = ch.predictor; v.step = ch.stepIndex; v.increment = ch.increment;
        v.active = 1;
        workerChannels[count++] = i;
    }
    if (!count) return;
    workerJob.mode |= count << 8;
    for (int32 i = count; i < MW_VOICES; i++) workerJob.voices[i].active = 0;
    workerResult()->magic = 0;
    SH2_DMA_CHCR0 = 0;
    SH2_DMA_SAR0 = uint32(&MARS_SYS_DMAFIFO);
    SH2_DMA_DAR0 = uint32(&workerStorage) | 0x20000000;
    SH2_DMA_TCR0 = (8 + count*sizeof(MWVoiceResult) + 7) / 8 * 4;
    SH2_DMA_DRCR0 = 0;
    SH2_DMA_CHCR0 = 0x44E1; // fixed source, increment destination, word, no IRQ
    const uint16* words = (const uint16*)&workerJob;
    uint32 wordCount = 2 + count*sizeof(MWVoice)/2;
    for (uint32 i = 0; i < wordCount; i += 3) {
        MARS_SYS_COMM2 = words[i];
        MARS_SYS_COMM12 = (uint32(i+1 < wordCount ? words[i+1] : 0) << 16) |
            (i+2 < wordCount ? words[i+2] : 0);
        MARS_SYS_COMM0 = MW_COMMAND + i/3;
        // Only descriptor acknowledgements are waited on. Decode completion is
        // asynchronous; late/stale results use the existing SH2 path instead.
        uint32 timeout = 2048;
        while (MARS_SYS_COMM0 && --timeout) { }
        if (!timeout) {
            SH2_DMA_CHCR0 = 0;
            gMarsWorkerMode = 0; gMarsWorkerFaults++;
            return;
        }
    }
    workerFrame = workerNow();
    workerBusy = true;
}

static __attribute__((section(".ramtext.audio"))) void mixBlock(volatile uint16* out)
{
    uint32 workerValid = collectWorker();
    int32 slots[VOICES] = {-1,-1,-1,-1};
    for (int32 i = 0; i < MW_VOICES; i++)
        if (workerValid & (1 << i)) slots[workerChannels[i]] = i;
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
    static ALIGN16 int32 accumulation[BLOCK_SIZE];
    fast_memset(accumulation, 0, BLOCK_SIZE);
    for (int32 i = 0; i < VOICES; i++)
    {
        Channel &ch = channels[i];
        if (!ch.data) continue;
        const int32 volume = ch.volume;
        const int32 increment = ch.increment;
        const int32 slot = slots[i];
        const volatile int16* pcm = slot >= 0 ? workerResult()->voices[slot].pcm : NULL;
        for (int32 n = 0; n < BLOCK_SIZE; n++)
        {
            int32 sample;
            if (pcm) sample = pcm[n];
            else {
                uint32 wanted = ch.position >> 8;
                while (ch.decoded <= wanted) decodeNext(ch);
                sample = ch.predictor;
            }
            accumulation[n] += (sample * volume) >> SND_VOL_SHIFT;
            ch.position += increment;
            if ((ch.position >> 8) >= ch.length) { ch.data = NULL; break; }
        }
        if (slot >= 0) {
            const volatile MWVoiceResult &r = workerResult()->voices[slot];
            ch.predictor = r.predictor;
            ch.decoded = r.decoded;
            ch.stepIndex = r.step;
        }
    }
    for (int32 n = 0; n < BLOCK_SIZE; n++) {
        int32 mixed = accumulation[n] >> 1; // 6 dB headroom, same integer rounding.
        if (mixed < -32768 || mixed > 32767) gMarsAudioClips++;
        mixed = X_CLAMP(mixed, -32768, 32767);
        out[n] = center + ((mixed * (center - 1)) >> 15);
    }
    queueWorker();
}

static void startDMA()
{
    SH2_DMA_SAR1 = uint32(audioBuffers[playBuffer]);
    SH2_DMA_TCR1 = BLOCK_SIZE;
    SH2_DMA_CHCR1 = 0x14E5; // word, increment source, fixed destination, external request + IRQ
}

extern "C" __attribute__((section(".ramtext.audio"))) void pri_dma1_handler()
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
    SH2_DMA_CHCR0 = 0;
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

volatile uint32 gMarsMixerTestCases, gMarsMixerTestErrors, gMarsMixerTestHash;
void marsMixerSelfTest()
{
    uint32 hash = 2166136261u;
    gMarsWorkerMode = 0;
    gSettings.audio_sfx = 1;
    period = 2048;
    const uint32 assets = sizeof(sampleAssets)/sizeof(sampleAssets[0]);
    for (uint32 test = 0; test < 64; test++) {
        for (int32 i = 0; i < VOICES; i++) {
            Channel &ch = channels[i];
            const MarsSampleAsset &a = sampleAssets[(test*7+i*13)%assets];
            ch.data = MARS_SFX_DATA+a.offset; ch.length = a.length;
            resetChannel(ch);
            ch.volume = 1+((test+i*19)%64);
            ch.increment = 32+((test+i*7)%32)*32;
        }
        for (int32 block = 0; block < 5; block++) {
            Channel reference[VOICES];
            for (int32 i = 0; i < VOICES; i++) reference[i] = channels[i];
            uint16 expected[BLOCK_SIZE], actual[BLOCK_SIZE];
            // Previous sample-major mixer, independent of the new voice-major loop.
            for (int32 n = 0; n < BLOCK_SIZE; n++) {
                int32 mixed = 0;
                for (int32 i = 0; i < VOICES; i++) {
                    Channel &ch = reference[i];
                    if (!ch.data) continue;
                    while (ch.decoded <= (ch.position>>8)) decodeNext(ch);
                    mixed += (ch.predictor*ch.volume)>>SND_VOL_SHIFT;
                    ch.position += ch.increment;
                    if ((ch.position>>8) >= ch.length) ch.data = NULL;
                }
                mixed = X_CLAMP(mixed>>1,-32768,32767);
                expected[n] = 1024+((mixed*1023)>>15);
            }
            mixBlock(actual);
            for (int32 n = 0; n < BLOCK_SIZE; n++) {
                if (expected[n] != actual[n]) gMarsMixerTestErrors++;
                hash = (hash ^ actual[n])*16777619u;
            }
            for (int32 i = 0; i < VOICES; i++) {
                const Channel &a=channels[i], &b=reference[i];
                if (a.data != b.data || a.position != b.position || a.decoded != b.decoded ||
                    a.predictor != b.predictor || a.stepIndex != b.stepIndex) gMarsMixerTestErrors++;
            }
            gMarsMixerTestCases++;
        }
    }
    gMarsMixerTestHash = hash;
}
#endif

#ifdef MARS_WORKER_SELFTEST
volatile uint32 gMarsWorkerTestCases, gMarsWorkerTestErrors, gMarsWorkerTestDone, gMarsWorkerTestFrames;
void marsWorkerSelfTest()
{
    SH2_DMA_DMAOR = 1;
    uint32 firstFrame = workerNow();
    const uint32 assets = sizeof(sampleAssets)/sizeof(sampleAssets[0]);
    for (uint32 test = 0; test < assets*3; test++) {
        for (int32 i = 0; i < VOICES; i++) channels[i].data = NULL;
        for (int32 i = 0; i < MW_VOICES; i++) {
            Channel &ch = channels[i];
            const MarsSampleAsset &a = sampleAssets[(test+i*17)%assets];
            ch.data = MARS_SFX_DATA + a.offset;
            ch.length = a.length;
            resetChannel(ch);
            uint32 first = test < assets ? 0 : (test < assets*2 ? 127 : a.length-1);
            if (first >= a.length) first = a.length-1;
            while (ch.decoded < first) decodeNext(ch);
            ch.position = (first << 8) | (test & 255);
            ch.increment = 32 + (test % 32)*32;
            ch.volume = 64;
        }
        queueWorker();
        if (!workerBusy) { gMarsWorkerTestErrors++; break; }
        uint32 start = workerNow();
        while (!(SH2_DMA_CHCR0 & 2) && workerNow()-start < 12) { }
        uint32 valid = collectWorker();
        if (valid != ((1u << MARS_WORKER_VOICES)-1)) gMarsWorkerTestErrors++;
        gMarsWorkerTestCases++;
        if (gMarsWorkerFaults) break;
    }
    gMarsWorkerTestErrors += gMarsWorkerVerifyErrors;
    gMarsWorkerTestFrames = workerNow()-firstFrame;
    gMarsWorkerTestDone = 1;
}
#endif
