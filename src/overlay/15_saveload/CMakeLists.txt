# 15_saveload, an overlay (15_saveload.bin in A_FILE). The source list is in link order.

psx_overlay(15_saveload FILE 15_saveload.bin REGION high
    SOURCES
        header.c
        saveload.c
    # Filler the linker leaves in the alignment gap after the .rodata
    GAP saveload.c .rodata 0xd0)
