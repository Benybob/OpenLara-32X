        .text
/*
        .global _TITLE_PKD
        .align  4
_TITLE_PKD:
        .long TITLE
TITLE:
        .incbin "data/TITLE.PKD"
*/
        .global _LEVEL1_PKD
        .align  4
_LEVEL1_PKD:
        .long LEVEL1
LEVEL1:
        .incbin "data/LEVEL1.PKD"

        .align  4
        .global _GYM_PKD
_GYM_PKD:
        .long GYM
GYM:
        .incbin "data/GYM.PKD"

        .align  4
        .global _LEVEL2_PKD
_LEVEL2_PKD:
        .long LEVEL2
LEVEL2:
        .incbin "data/LEVEL2.PKD"

        .align  4
