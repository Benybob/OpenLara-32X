#define tmp     r0
#define Lh      r1
#define Rh      r2
#define dup     r3
#define pixel   r4      // arg
#define L       r5      // arg
#define R       r6      // arg
#define gtile   r7      // arg
#define N       gtile
#define Lx      r8
#define Rx      r9
#define Lg      r10
#define Rg      r11
#define Lt      r12
#define Rt      r13
#define TILE    r14     // const

#define h       N

#define Ldx     h
#define Rdx     h

#define Ldt     h
#define Rdt     h

#define Ry      Rx
#define Ly      Lx

#define Rv      Rx
#define Lv      Lx

#define Lptr    Lh
#define Rptr    Rx

#define g       Rg
#define dgdx    R

#define t       Rt
#define dtdx    L

#define index   tmp
#define LMAP    dup

#define divLUT  dup
#define iw      dup
#define ih      dup

#define dx      dgdx
#define mask    Rh

#define sLdx    L
#define sRdx    R
#define sLdt    L
#define sRdt    R
#define sLdg    L
#define sRdg    R

#define SP_LDX  0
#define SP_RDX  4
#define SP_LDT  8
#define SP_RDT  12
#define SP_LDG  16
#define SP_RDG  18
#define SP_H    20
#define SP_L    24
#define SP_R    28
#define SP_SIZE 32

.align 4
.exit_gt:
        // pop
        add     #SP_SIZE, sp
        mov.l   @sp+, r14
        mov.l   @sp+, r13
        mov.l   @sp+, r12
        mov.l   @sp+, r11
        mov.l   @sp+, r10
        mov.l   @sp+, r9
        rts
        mov.l   @sp+, r8
        nop

.global _rasterizeGT_asm
_rasterizeGT_asm:
        // push
        mov.l   r8, @-sp
        mov.l   r9, @-sp
        mov.l   r10, @-sp
        mov.l   r11, @-sp
        mov.l   r12, @-sp
        mov.l   r13, @-sp
        mov.l   r14, @-sp
        add     #-SP_SIZE, sp

        mov     gtile, TILE
        mov     #0, Rh

.loop_gt:
        extu.w  Rh, Lh  // Lh = int16(Rh)

        tst     Lh, Lh
        bf/s    .calc_left_end_gt
        shlr16  Rh              // [delay slot] Rh = (Rh >> 16)

.calc_left_start_gt:
        mov.b   @(VERTEX_PREV, L), tmp
        mov     tmp, N

        mov.w   @(VERTEX_Y, L), tmp
        add     L, N            // N = L + (L->prev << VERTEX_SIZEOF_SHIFT)
        mov     tmp, Ly
        mov.w   @(VERTEX_Y, N), tmp
        sub     Ly, tmp
        cmp/pz  tmp
        bf/s    .exit_gt
        tst     tmp, tmp
        mov     L, Lv           // Lv = L
        bt/s    .calc_left_start_gt      // if (Lh == 0) check next vertex
        mov     N, L            // [delay slot]

        mov     tmp, Lh
        mov.b   @(VERTEX_G, Lv), tmp
        mov.l   @(VERTEX_T, Lv), Lt
        mov     tmp, Lg
        add     #-1, Lg        // Encoded shades 1..32 become rows 0..31.
        mov.w   @(VERTEX_X, Lv), tmp
          shll8   Lg
        swap.w  tmp, Lx         // Lx = L->v.x << 16

        mov     Lh, tmp
        cmp/eq  #1, tmp
        bt/s    .calc_left_end_gt
        shll    tmp             // [delay slot]

        mov.l   var_divTable, divLUT
        mov.w   @(tmp, divLUT), ih

        // calc Ldx
        mov.w   @(VERTEX_X, L), tmp
          swap.w  Lx, Ldx
        sub     Ldx, tmp
        muls.w  ih, tmp
          mov.b   @(VERTEX_G, L), tmp
        sts     MACL, Ldx
          add     #-1, tmp
          shll8   tmp
        mov.l   Ldx, @(SP_LDX, sp)

        // calc Ldg
        sub     Lg, tmp
        muls.w  ih, tmp
          mov.l   @(VERTEX_T, L), Ldt
        sts     MACL, tmp
          sub     Lt, Ldt
        shlr16  tmp
        mov.w   tmp, @(SP_LDG, sp)

        // calc Ldt
        scaleUV Ldt, tmp, ih
        mov.l   tmp, @(SP_LDT, sp)
        nop
.calc_left_end_gt:

        tst     Rh, Rh
        bf/s    .calc_right_end_gt

.calc_right_start_gt:
        mov.b   @(VERTEX_NEXT, R), tmp  // [delay slot]
        mov     tmp, N

        mov.w   @(VERTEX_Y, R), tmp
        add     R, N            // N = R + (R->next << VERTEX_SIZEOF_SHIFT)
        mov     tmp, Ry
        mov.w   @(VERTEX_Y, N), tmp
        sub     Ry, tmp
        cmp/pz  tmp
        bf/s    .exit_gt
        tst     tmp, tmp
        mov     R, Rv           // Rv = R
        bt/s    .calc_right_start_gt     // if (Rh == 0) check next vertex
        mov     N, R            // [delay slot]

        mov     tmp, Rh
        mov.b   @(VERTEX_G, Rv), tmp
        mov.l   @(VERTEX_T, Rv), Rt
        mov     tmp, Rg
        add     #-1, Rg
        mov.w   @(VERTEX_X, Rv), tmp
          shll8   Rg
        swap.w  tmp, Rx         // Rx = R->v.x << 16

        mov     Rh, tmp
        cmp/eq  #1, tmp
        bt/s    .calc_right_end_gt
        shll    tmp             // [delay slot]

        mov.l   var_divTable, divLUT
        mov.w   @(tmp, divLUT), ih

        // calc Rdx
        mov.w   @(VERTEX_X, R), tmp
          swap.w  Rx, Rdx
        sub     Rdx, tmp
        muls.w  ih, tmp
          mov.b   @(VERTEX_G, R), tmp
        sts     MACL, Rdx
          add     #-1, tmp
          shll8   tmp
        mov.l   Rdx, @(SP_RDX, sp)

        // calc Rdg
        sub     Rg, tmp
        muls.w  ih, tmp
          mov.l   @(VERTEX_T, R), Rdt
        sts     MACL, tmp
          sub     Rt, Rdt
        shlr16  tmp
        mov.w   tmp, @(SP_RDG, sp)

        // calc Rdt
        scaleUV Rdt, tmp, ih
        mov.l   tmp, @(SP_RDT, sp)
        nop
.calc_right_end_gt:

        // Embed half the aligned pair-table address, keeping 8-bit shade fractions.
        // Doubling the masked address below selects the duplicated-color row.
        mov.l   var_LMAP_PAIRS_gt, tmp
        or      tmp, Lg
        or      tmp, Rg

        // h = min(Lh, Rh)
        cmp/gt  Rh, Lh
        bf/s    .scanline_prepare_gt
        mov     Lh, h           // [delay slot]
        mov     Rh, h
        nop

.scanline_prepare_gt:
        sub     h, Lh
        sub     h, Rh

        swap.w  Rh, tmp
        or      Lh, tmp

        mov.l   tmp, @(SP_H, sp)
        mov.l   L, @(SP_L, sp)
        mov.l   R, @(SP_R, sp)

        mov     #-1, mask
        shll8   mask
        
.scanline_start_gt:
        mov.l   Rx, @-sp        // alias Rptr

        mov     Lx, Lptr
        shlr16  Lptr            // Lptr = (Lx >> 16)
        shlr16  Rptr            // Rptr = (Rx >> 16)
        cmp/gt  Lptr, Rptr      // if (!(Rptr > Lptr)) skip zero length scanline
        bt/s    .scanline_nonempty_gt

        // iw = divTable[Rptr - Lptr]
        mov     Rptr, tmp       // [delay slot]
        bra     .scanline_end_fast_gt
        nop
.scanline_nonempty_gt:
        sub     Lptr, tmp
        mov.l   var_divTable, divLUT
        shll    tmp
        mov.w   @(tmp, divLUT), iw

        add     pixel, Lptr     // Lptr = pixel + (Lx >> 16)
        add     pixel, Rptr     // Rptr = pixel + (Rx >> 16)

        mov.l   Rt, @-sp        // alias t
        mov.l   Rg, @-sp        // alias g

        // calc dtdx
        mov     Rt, tmp
        sub     Lt, tmp
        muls.w  tmp, iw
          shlr16  tmp
        sts     MACL, dtdx      // v = int16(uv) * f (16-bit shift)
        muls.w  tmp, iw
          mov     Rg, tmp
        sts     MACL, dx        // u = int16(uv >> 16) * f (16-bit shift)
          sub     Lg, tmp
        shlr16  dx
        xtrct   dx, dtdx        // out = uint16(v >> 16) | (u & 0xFFFF0000)

        // calc dgdx
        muls.w  tmp, iw
          mov     #1, tmp
        sts     MACL, dgdx
          tst     tmp, Lptr
        shlr16  dgdx
        exts.w  dgdx, dgdx

.align_left_gt:
        bt/s    .align_right_gt
        tst     tmp, Rptr       // [delay slot]

        getUV   Lt, index
        mov.b   @(index, TILE), index
        mov     Lg, LMAP
        and     mask, LMAP
        shll    LMAP
        shll    index
        mov.w   @(index, LMAP), index

        mov.b   index, @Lptr
        add     #1, Lptr

        mov     #1, tmp         // tmp = 1 (for align_right)
        cmp/gt  Lptr, Rptr
        bt/s    .align_right_gt
        tst     tmp, Rptr
        bra     .scanline_end_gt
        nop

.align_right_gt:
        bt/s    .block_prepare_gt
        mov     g, LMAP

        getUV   t, index
        mov.b   @(index, TILE), index
        and     mask, LMAP
        sub     dgdx, g
        shll    LMAP
        shll    index
        mov.w   @(index, LMAP), index

        sub     dtdx, t

        mov.b   index, @-Rptr

        cmp/gt  Lptr, Rptr
        bt      .block_prepare_gt
        bra     .scanline_end_gt
        nop

.block_prepare_gt:
        shll    dtdx
        shll    dgdx
        tst     dgdx, dgdx
        bf      .gradient_span_gt
        bra     .constant_span_gt
        nop
.gradient_span_gt:

        // Preserve the original texture and shade sample for each pixel pair.
        add     #14, Lptr
        cmp/gt  Lptr, Rptr
        bf      .block_tail_gt
.block_16px_gt:
        .rept 8
        getUV   t, index
        mov.b   @(index, TILE), index
        mov     g, LMAP
        and     mask, LMAP
        shll    LMAP
        shll    index
        mov.w   @(index, LMAP), index
        sub     dgdx, g
        mov.w   index, @-Rptr
        sub     dtdx, t
        .endr
        cmp/gt  Lptr, Rptr
        bt      .block_16px_gt
.block_tail_gt:
        add     #-14, Lptr
        cmp/gt  Lptr, Rptr
        bf      .scanline_end_gt

.block_2px_gt:
        getUV   t, index

        mov.b   @(index, TILE), index

        mov     g, LMAP
        and     mask, LMAP      // LMAP = (g & 0xFFFFFF00)
        shll    LMAP
        shll    index
        mov.w   @(index, LMAP), index
        sub     dgdx, g         // g -= dgdx

        mov.w   index, @-Rptr

        cmp/gt  Lptr, Rptr
        bt/s    .block_2px_gt
        sub     dtdx, t         // [delay slot] t -= dtdx

        bra     .scanline_end_gt
        nop

// A zero horizontal shade step keeps the lookup row fixed for this span.
// UV stepping, word order and odd endpoints remain identical to the GT path.
.constant_span_gt:
        mov     g, LMAP
        and     mask, LMAP
        shll    LMAP
        add     #6, Lptr
        cmp/gt  Lptr, Rptr
        bf      .constant_tail_gt
.constant_batch_gt:
        .rept 4
        getUV   t, index
        mov.b   @(index, TILE), index
        sub     dtdx, t
        shll    index
        mov.w   @(index, LMAP), index
        mov.w   index, @-Rptr
        .endr
        cmp/gt  Lptr, Rptr
        bt      .constant_batch_gt
.constant_tail_gt:
        add     #-6, Lptr
        cmp/gt  Lptr, Rptr
        bf      .scanline_end_gt
.constant_pair_gt:
        getUV   t, index
        mov.b   @(index, TILE), index
        sub     dtdx, t
        shll    index
        mov.w   @(index, LMAP), index
        mov.w   index, @-Rptr
        cmp/gt  Lptr, Rptr
        bt      .constant_pair_gt

.scanline_end_gt:
        mov.l   @sp+, Rg
        mov.l   @sp+, Rt
.scanline_end_fast_gt:
        mov.l   @sp+, Rx

        mov     sp, tmp

        mov.l   @tmp+, sLdx
        mov.l   @tmp+, sRdx

        add     sLdx, Lx
        add     sRdx, Rx

        mov.l   @tmp+, sLdt
        mov.l   @tmp+, sRdt

        add     sLdt, Lt
        add     sRdt, Rt

        mov.w   @tmp+, sLdg
        mov.w   @tmp+, sRdg

        add     sLdg, Lg
        add     sRdg, Rg

        dt      h

        mov.w   var_frameWidth, tmp
        bt/s    .scanline_block_done_gt
        add     tmp, pixel      // [delay slot] pixel += 120 + 120 + 80
        bra     .scanline_start_gt
        nop
.scanline_block_done_gt:

        mov.l   @(SP_L, sp), L
        mov.l   @(SP_R, sp), R
        bra     .loop_gt
        mov.l   @(SP_H, sp), Rh

.align 2
var_LMAP_PAIRS_gt:
        .long _gLightmapPairsHalf

#undef tmp
#undef Lh
#undef Rh
#undef dup
#undef pixel
#undef L
#undef R
#undef N
#undef Lx
#undef Rx
#undef Lg
#undef Rg
#undef Lt
#undef Rt
#undef TILE
#undef h
#undef Ldx
#undef Rdx
#undef Ldt
#undef Rdt
#undef Ry
#undef Ly
#undef Rv
#undef Lv
#undef Lptr
#undef Rptr
#undef g
#undef dgdx
#undef t
#undef dtdx
#undef index
#undef LMAP
#undef divLUT
#undef iw
#undef ih
#undef dx
#undef mask
#undef sLdx
#undef sRdx
#undef sLdt
#undef sRdt
#undef sLdg
#undef sRdg
#undef SP_LDX
#undef SP_RDX
#undef SP_LDT
#undef SP_RDT
#undef SP_LDG
#undef SP_RDG
#undef SP_H
#undef SP_L
#undef SP_R
#undef SP_SIZE
