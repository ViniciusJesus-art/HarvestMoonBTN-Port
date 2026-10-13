# 31_gf_mcard, an overlay (31_gf_mcard.bin in A_FILE). The source list is in link order.

psx_overlay(31_gf_mcard FILE 31_gf_mcard.bin REGION low
    SOURCES
        header.c
        mcard.c
    # Filler the linker leaves in the alignment gap after the .rodata
    GAP mcard.c .rodata 0x87)
