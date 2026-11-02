        .section .vectors,"a"
        .long 0x0007FFF0,cd_worker_start
        .rept 62
        .long cd_worker_exception
        .endr
        .text
        .global cd_worker_start
cd_worker_start:
        move.w #0x2700,sr
        jsr cd_worker_main
cd_worker_exception:
        bra.s cd_worker_exception
