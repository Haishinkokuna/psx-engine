/*******************************************************************************
 * FILE:         mipsel-toolchain.cmake
 * MODULE:       Build System / Cross-Compile
 * DESCRIPTION:  CMake toolchain file for cross-compiling psx-runtime to the
 *               MIPS R3000A little-endian target using the mipsel-unknown-elf
 *               GCC toolchain (distributed with PSn00bSDK or built from source
 *               via the GNU MIPS ELF cross-compiler build scripts).
 *
 *               This file is passed to CMake via:
 *                 cmake -DCMAKE_TOOLCHAIN_FILE=cmake/mipsel-toolchain.cmake
 *
 *               CRITICAL COMPILER FLAGS EXPLAINED:
 *
 *                 -march=r3000       Target the MIPS I ISA (R3000 instruction
 *                                   set). Prevents GCC from emitting R4000+
 *                                   instructions that the PSX CPU cannot decode.
 *
 *                 -msoft-float       MANDATORY. Makes GCC emit software float
 *                                   emulation stubs instead of FPU instructions.
 *                                   More importantly: if any float or double
 *                                   type slips through to runtime code, the
 *                                   linker will ERROR on missing soft-float
 *                                   stubs unless libgcc is linked. This acts
 *                                   as a mechanical enforcement of the no-float
 *                                   law — the build BREAKS if floats exist.
 *
 *                 -mno-abicalls      Disable MIPS PIC calling convention. The
 *                                   PSX uses a flat binary — there is no shared
 *                                   library loader. PIC adds GOT overhead we
 *                                   do not want.
 *
 *                 -G0                Set the GP-relative small data threshold
 *                                   to 0. Disables small-data optimization
 *                                   (which uses $gp register) to avoid
 *                                   conflicts with the PSX runtime's use of
 *                                   that register for its own purposes.
 *
 *                 -O2                Standard optimization. Do NOT use -O3
 *                                   unless you have verified that aggressive
 *                                   auto-vectorization does not produce illegal
 *                                   instructions on R3000A.
 *
 *                 -fno-builtin       Prevent GCC from replacing calls like
 *                                   memcpy with inline SIMD that does not
 *                                   exist on this CPU.
 *
 * DEPENDENCIES: mipsel-unknown-elf-gcc, mipsel-unknown-elf-g++,
 *               mipsel-unknown-elf-ar must all be on PATH.
 *******************************************************************************/

# Tell CMake this is a cross-compile — it will not attempt to run test
# executables on the host machine during the configure step.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR mips)

# ---------------------------------------------------------------------------
# Toolchain executables
# Override these via -DMIPSEL_TOOLCHAIN_PREFIX if your installation uses a
# different prefix (e.g., mipsel-linux-gnu- from PSn00bSDK).
# ---------------------------------------------------------------------------

if(NOT DEFINED MIPSEL_TOOLCHAIN_PREFIX)
    set(MIPSEL_TOOLCHAIN_PREFIX "mipsel-unknown-elf-")
endif()

set(CMAKE_C_COMPILER   "${MIPSEL_TOOLCHAIN_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${MIPSEL_TOOLCHAIN_PREFIX}g++")
set(CMAKE_AR           "${MIPSEL_TOOLCHAIN_PREFIX}ar")
set(CMAKE_RANLIB       "${MIPSEL_TOOLCHAIN_PREFIX}ranlib")
set(CMAKE_OBJCOPY      "${MIPSEL_TOOLCHAIN_PREFIX}objcopy")
set(CMAKE_STRIP        "${MIPSEL_TOOLCHAIN_PREFIX}strip")

# ---------------------------------------------------------------------------
# Compiler flags common to C and C++
# ---------------------------------------------------------------------------

set(PSX_COMMON_FLAGS
    "-march=r3000"       # Target MIPS I / R3000 instruction set
    "-msoft-float"       # No FPU — software float stubs; breaks build if float used
    "-mno-abicalls"      # Flat binary — no PIC/GOT overhead
    "-G0"                # Disable GP-relative small data area
    "-O2"                # Standard optimization level
    "-fno-builtin"       # No GCC auto-replacement of stdlib calls
    "-fno-exceptions"    # No C++ exception tables in PSX code (saves code size)
    "-fno-rtti"          # No RTTI type info tables (saves RAM)
    "-Wall"              # All standard warnings
    "-Wextra"            # Extra warnings (catches signed/unsigned mismatches)
    "-Werror=float-conversion"  # Treat implicit float conversions as errors
    "-Werror=double-promotion"  # Treat double promotions as errors
)

# Convert the list to a space-separated string for CMake flag variables
list(JOIN PSX_COMMON_FLAGS " " PSX_COMMON_FLAGS_STR)

set(CMAKE_C_FLAGS   "${PSX_COMMON_FLAGS_STR}")
set(CMAKE_CXX_FLAGS "${PSX_COMMON_FLAGS_STR} -std=c++17")

# ---------------------------------------------------------------------------
# Linker flags
# ---------------------------------------------------------------------------

# Tell the linker we are producing a flat ELF, not a shared object.
# The PSX ELF is then converted to a raw binary by ps-exe or objcopy.
set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -Wl,--nmagic")

# ---------------------------------------------------------------------------
# Sysroot (optional)
# If PSn00bSDK is installed, point CMAKE_SYSROOT at its sysroot to pick up
# the SDK's libc stubs, startup code, and hardware abstraction layer.
# Comment this out if building a fully bare-metal engine without any SDK.
# ---------------------------------------------------------------------------

# set(CMAKE_SYSROOT "/opt/psn00bsdk/mipsel-unknown-elf")
# set(CMAKE_FIND_ROOT_PATH "/opt/psn00bsdk/mipsel-unknown-elf")
# set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
