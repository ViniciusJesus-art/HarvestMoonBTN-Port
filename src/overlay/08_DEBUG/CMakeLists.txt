# 08_DEBUG, an overlay (08_DEBUG.bin in A_FILE). The source list is in link order.

psx_overlay(08_DEBUG FILE 08_DEBUG.bin REGION low
    SOURCES
        header.c
        debug.c
    # Filler the linker leaves in the alignment gap after the .rodata
    GAP debug.c .rodata 0x78 0x46 0x45)
