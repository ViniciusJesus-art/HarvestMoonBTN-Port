# The PlayStation toolchain the game is built with.
#
# GCC 2.95.2's cpp and cc1 (byte-identical to SN Systems' CC1PSX), maspsx in front
# of GNU as (in place of ASPSX), and the MIPS GNU binutils. The defaults are where
# the build image has them; override any on the command line (-DPSX_GCC_DIR=...).

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR mipsel)

set(PSX_GCC_DIR /opt/psyq/gcc-2.95.2 CACHE PATH "GCC 2.95.2 (cpp, cc1) for the PlayStation")
set(PSX_BINUTILS_PREFIX mips-linux-gnu- CACHE STRING "Prefix of the MIPS GNU binutils")
set(PSYQ_SDK_DIR /opt/psyq/sdk-4.7 CACHE PATH "PSY-Q Runtime Library 4.7 (its LIB/OBJ files)")
set(PSYQ_INCLUDE_DIR /opt/psyq/include CACHE PATH "PSY-Q 4.7 headers, lower-case file names")

set(PSX_CPP ${PSX_GCC_DIR}/cpp)
set(PSX_CC1 ${PSX_GCC_DIR}/cc1)
set(PSX_AS ${PSX_BINUTILS_PREFIX}as)
set(PSX_LD ${PSX_BINUTILS_PREFIX}ld)
set(PSX_OBJCOPY ${PSX_BINUTILS_PREFIX}objcopy)
