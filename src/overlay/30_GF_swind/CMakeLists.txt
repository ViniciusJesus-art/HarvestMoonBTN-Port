# 30_GF_swind, an overlay (30_GF_swind.bin in A_FILE). The source list is in link order.

psx_overlay(30_GF_swind FILE 30_GF_swind.bin REGION high
    SOURCES
        header.c
        swind.c
    # Filler the linker leaves in the alignment gap after the .rodata
    GAP swind.c .rodata 0x52 0x8d)
