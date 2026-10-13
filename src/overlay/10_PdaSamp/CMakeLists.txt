# 10_PdaSamp, an overlay (10_PdaSamp.bin in A_FILE). The source list is in link order.

psx_overlay(10_PdaSamp FILE 10_PdaSamp.bin REGION low
    SOURCES
        header.c
        pdasamp.c
    # Filler the linker leaves in the alignment gap after the .rodata
    GAP pdasamp.c .rodata 0x2c 0x0d 0x0a)
