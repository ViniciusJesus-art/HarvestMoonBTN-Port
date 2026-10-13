#ifndef FARM_DATA_SIZE_H
#define FARM_DATA_SIZE_H


/* The size of the farm data section FARM loads, and the word after it. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"

/* The size of the farm data section. */
extern s32 farm_section_size;
extern s32 farm_swind_size;

#endif /* FARM_DATA_SIZE_H */
