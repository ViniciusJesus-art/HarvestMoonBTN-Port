#ifndef COMPILED_OUT_H
#define COMPILED_OUT_H

/* The debug macros the release build leaves out. */

#define DEBUG_PRINT_BUF(buffer) do { } while (0)

/* A debug check that the release build leaves out. */
#define FARM_DEBUG_CHECK() do { } while (0)

#endif /* COMPILED_OUT_H */
