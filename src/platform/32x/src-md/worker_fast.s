| Exact IMA decode/resample, shared by stock Genesis and Sega CD 68000s.
| Descriptor ROM member is a mapped CPU-local address, not a bus address.
        .section .ramtext,"ax"
        .global mwDecodeFast
mwDecodeFast:
        movem.l d2-d7/a2-a6,-(sp)
        movea.l 48(sp),a2
        movea.l 52(sp),a3
        move.l 8(a2),d2
        move.l 12(a2),d3
        move.w 16(a2),d4
        ext.l d4
        moveq #0,d6
        move.w 18(a2),d6
        lsl.w #7,d6
        moveq #0,d7
        move.w 20(a2),d7
        lea mwTransitions,a4
        move.l d3,d0
        lsr.l #1,d0
        add.l (a2),d0
        movea.l d0,a5
        lea 12(a3),a6
        moveq #0,d1
        btst #0,d3
        beq.s 0f
        move.b (a5),d1
0:
        move.w #256,-(sp)
1:
        move.l d2,d5
        lsr.l #8,d5
        cmp.l 4(a2),d5
        bcc.s 7f
        movea.l d5,a0
        cmp.l a0,d3
        bhi.s 5f
2:
        btst #0,d3
        bne.s 3f
        move.b (a5),d1
        move.w d1,d0
        and.w #15,d0
        bra.s 4f
3:
        move.w d1,d0
        lsr.w #4,d0
        and.w #15,d0
        addq.l #1,a5
4:
        lsl.w #3,d0
        add.w d6,d0
        move.l (a4,d0.w),d5
        add.l d5,d4
        move.w d4,d5
        ext.l d5
        cmp.l d4,d5
        beq.s 8f
        tst.l d4
        bmi.s 9f
        move.l #32767,d4
        bra.s 8f
9:
        move.l #-32768,d4
8:
        move.w 4(a4,d0.w),d6
        addq.l #1,d3
        cmp.l a0,d3
        bls.s 2b
5:
        move.w d4,(a6)+
        add.l d7,d2
6:
        subq.w #1,(sp)
        bne.s 1b
        addq.l #2,sp
        move.l d2,(a3)
        move.l d3,4(a3)
        move.w d4,8(a3)
        lsr.w #7,d6
        move.w d6,10(a3)
        movem.l (sp)+,d2-d7/a2-a6
        rts
7:
        clr.w (a6)+
        bra.s 6b

        .section .data.mwfast,"aw"
        .global mwTransitions
mwTransitions:
        .include "worker_fast_tables.inc"
