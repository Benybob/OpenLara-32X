// Optional on-target regression test. Build with CPPFLAGS=-DMARS_RASTER_SELFTEST.
// Run the same file against upstream and modified rasterizers and compare hashes.
volatile uint32 gRasterTestCases = 0;
volatile uint32 gRasterTestFT = 2166136261u;
volatile uint32 gRasterTestGT = 2166136261u;
volatile uint32 gRasterTestDone = 0;
volatile uint32 gRasterTestMathErrors = 0;

volatile uint32 gRasterTestShadeCases = 0;
volatile uint32 gRasterTestShades = 2166136261u;

volatile uint32 gClipTestCases = 0;
volatile uint32 gClipTestHash = 2166136261u;

void marsRasterSelfTest()
{
    MARS_WAIT();
    const ColorIndex* tile = (const ColorIndex*)level.textures[0].tile;
    VertexLink v[4] = {};
    for (int32 i = 0; i < 4; i++) {
        v[i].prev = i ? -16 : 48;
        v[i].next = i == 3 ? -48 : 16;
    }
    v[0].t.t = 0;
    v[1].t.t = 0x7F000000;
    v[2].t.t = 0x7F007F00;
    v[3].t.t = 0x00007F00;
    for (int32 type = 0; type < 2; type++)
    for (int32 odd = 0; odd < 2; odd++)
    for (int32 width = 1; width <= FRAME_WIDTH - odd; width++)
    for (int32 slope = 0; slope < 2; slope++)
    {
        fast_memset(fb, 0, FRAME_WIDTH * 6 / 4);
        v[0].v.x = odd;                 v[0].v.y = 0;
        v[1].v.x = odd + width;         v[1].v.y = 0;
        v[2].v.x = odd + width - slope * (width / 4); v[2].v.y = 4;
        v[3].v.x = odd + slope * (width / 4);         v[3].v.y = 4;
        v[0].v.g = 1 + width % 20;
        v[1].v.g = 5 + width % 20;
        v[2].v.g = 9 + width % 20;
        v[3].v.g = 3 + width % 20;
        uint16* pixel = (uint16*)((uint8*)&MARS_OVERWRITE_IMG + 0x200);
        if (type) rasterizeGT_asm(pixel, v, v, tile);
        else      rasterizeFT_asm(pixel, v, v, tile);
        uint32 hash = type ? gRasterTestGT : gRasterTestFT;
        const volatile uint8* pixels = (const volatile uint8*)fb;
        for (int32 i = 0; i < FRAME_WIDTH * 6; i++) {
            hash = (hash ^ pixels[i]) * 16777619u;
        }
        if (type) gRasterTestGT = hash;
        else      gRasterTestFT = hash;
        gRasterTestCases++;
    }
    // Boundary shades and both gradient directions exercise the new table's
    // first/last rows, fractional interpolation, odd endpoints and loop tails.
    for (int32 shade = 0; shade < 4; shade++)
    for (int32 odd = 0; odd < 2; odd++)
    for (int32 width = 1; width <= FRAME_WIDTH - odd; width++)
    for (int32 slope = 0; slope < 2; slope++)
    {
        fast_memset(fb, 0, FRAME_WIDTH * 6 / 4);
        v[0].v.x = odd; v[0].v.y = 0;
        v[1].v.x = odd + width; v[1].v.y = 0;
        v[2].v.x = odd + width - slope * (width / 4); v[2].v.y = 4;
        v[3].v.x = odd + slope * (width / 4); v[3].v.y = 4;
        v[0].v.g = v[3].v.g = (shade & 1) ? 32 : 1;
        v[1].v.g = v[2].v.g = (shade & 2) ? 32 : 1;
        rasterizeGT_asm((uint16*)((uint8*)&MARS_OVERWRITE_IMG + 0x200), v, v, tile);
        const volatile uint8* pixels = (const volatile uint8*)fb;
        uint32 hash = gRasterTestShades;
        for (int32 i = 0; i < FRAME_WIDTH * 6; i++) hash = (hash ^ pixels[i]) * 16777619u;
        gRasterTestShades = hash;
        gRasterTestShadeCases++;
    }
    uint32 seed = 0x3210ABCDu;
    for (int32 i = 0; i < 4096; i++)
    {
        seed = seed * 1664525u + 1013904223u;
        volatile int32 n = int32(seed);
        seed = seed * 1664525u + 1013904223u;
        volatile int32 d = int32(seed | 3u); // nonzero, never -1
        if (marsDivide(n, d) != n / d) gRasterTestMathErrors++;
    }
    const int xs[] = {-100, 0, 80, 319, 321};
    const int ys[] = {-50, 0, 100, 223, 225};
    const int widths[] = {20, 80, 400};
    const int heights[] = {30, 150, 300};
    for (int type = 0; type < 2; type++)
    for (int tri = 0; tri < 2; tri++)
    for (int xi = 0; xi < 5; xi++)
    for (int yi = 0; yi < 5; yi++)
    for (int wi = 0; wi < 3; wi++)
    for (int hi = 0; hi < 3; hi++) {
        fast_memset(fb, 0, FRAME_WIDTH * FRAME_HEIGHT / 4);
        int x = xs[xi], y = ys[yi], w = widths[wi], h = heights[hi];
        v[0].v.x = x; v[0].v.y = y;
        v[1].v.x = x+w; v[1].v.y = y;
        v[2].v.x = x+w; v[2].v.y = y+h;
        v[3].v.x = x; v[3].v.y = y+h;
        v[0].v.g = 1; v[1].v.g = 32; v[2].v.g = 21; v[3].v.g = 11;
        v[0].t.t = 0; v[1].t.t = 0x7F000000;
        v[2].t.t = 0x7F007F00; v[3].t.t = 0x00007F00;
        drawPoly(((type ? FACE_TYPE_GT : FACE_TYPE_FT) << FACE_TYPE_SHIFT) |
            (tri ? FACE_TRIANGLE : 0) | FACE_CLIPPED, v, tile);
        const volatile uint8* pixels = (const volatile uint8*)fb;
        uint32 hash = gClipTestHash;
        for (int i = 0; i < FRAME_WIDTH * FRAME_HEIGHT; i++)
            hash = (hash ^ pixels[i]) * 16777619u;
        gClipTestHash = hash;
        gClipTestCases++;
    }
    gRasterTestDone = 1;
}
