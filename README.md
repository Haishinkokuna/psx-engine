# PSX Engine

An open-source, simplified Unreal-style game development environment targeting original 1994 PlayStation 1 hardware. Developers build games using modern C++ or a visual node-based editor, which transpiles and compiles into a raw binary capable of running natively on a PS1 console.

An embedded Model Context Protocol (MCP) server allows external AI agents to programmatically control the desktop editor, manipulate scenes, and automate asset pipelines via JSON-RPC 2.0.

---

## Repository Structure

```
psx-engine/
├── .antigravity/          Core agent instructions (system prompt, coding laws)
├── psx-editor/            Modern PC desktop application (Dear ImGui / SDL2 / OpenGL3)
├── psx-runtime/           Core PSX C/C++ engine runtime (MIPS R3000A target)
├── psx-mcp-server/        JSON-RPC 2.0 MCP server layer for AI agent control
└── tools/                 Asset compilers, ISO builders, utility scripts
```

---

## Target Hardware Specifications

| Component | Specification |
|-----------|---------------|
| CPU | MIPS R3000A at 33.8688 MHz with GTE (Coprocessor 2) |
| Main RAM | 2 MB |
| VRAM | 1 MB (1024x512 at 15bpp / R5G5B5) |
| Scratchpad | 1 KB CPU Data Cache (0x1F800000), single-cycle access |
| FPU | None. All math is fixed-point integer arithmetic |
| Storage | CD-ROM (ISO 9660), optimized streaming |

**There are zero floats or doubles in any PSX runtime code.** The `-msoft-float` compiler flag enforces this mechanically — any accidental float usage causes a linker error rather than silently compiling incorrect code.

---

## Current Modules

### psx-runtime/core/math

Fixed-point math library. The foundation every other runtime module depends on.

- `fixed.h` — `FP4_12` and `FP12_4` type aliases, multiply/divide macros using `int64_t` intermediates to prevent overflow, conversion and clamping utilities.
- `vec3.h` — Three-component fixed-point vector (`FP12_4`, GTE VECTOR layout). Add, subtract, dot product, cross product, scale, lerp, squared length.
- `mat3.h` — 3x3 rotation matrix (`int16_t[9]`, GTE MATRIX layout, FP4_12 components). RotX, RotY, RotZ, RotXYZ composition, multiply, transpose.
- `trig.h` / `trig.c` — 256-entry `int16_t` sine lookup table in FP4_12 format (512 bytes in ROM, zero RAM cost at runtime). `BAM8` angle type (Binary Angular Measurement, 0-255 = full circle). `FP_Sin`, `FP_Cos`, `FP_SinCos` inline accessors.

### psx-runtime/core/memory

Two completely separate allocators with no shared state.

- `mem_map.h` — Every PSX hardware address constant (`PSX_SCRATCH_BASE`, `PSX_HEAP_BASE`, GPU, SPU, DMA, VRAM layout). Single source of truth — no magic numbers anywhere else in the codebase.
- `scratch.h` / `scratch.c` — Scratchpad bump allocator at `0x1F800000`. 4-byte aligned allocations, frame-level `Scratch_Reset()` semantics, overflow protection with NULL return.
- `heap.h` / `heap.c` — Main RAM first-fit freelist over approximately 1.875 MB. In-band block headers (8 bytes overhead), `HEAP_MAGIC` double-free detection, forward and backward coalescing on free.

### psx-editor

Desktop application running on the developer's PC. It does NOT produce PSX binary output directly — that is the cross-compiler toolchain's job. The editor provides:

- A Dear ImGui docking interface (three panels: Scene Viewport, Inspector, Console).
- An OpenGL 3.3 PSX artifact simulation renderer:
  - **Vertex snapping** — geometry snapped to the 320x240 integer grid (the source of classic PSX "wobbly" polygon swimming).
  - **Affine UV mapping** — texture coordinates interpolated without perspective division (`noperspective` GLSL qualifier), replicating the PSX GPU's linear interpolation.
  - **15bpp color quantization** — R5G5B5 framebuffer simulation (32 levels per channel).
  - **4x4 Bayer dithering** — optional, matches hardware dithering pass.
- A placeholder debug cube to demonstrate the artifact pipeline before scene data exists.
- MCP server integration (see below).

### psx-mcp-server

JSON-RPC 2.0 server that runs as a thread inside the editor process. External AI agents communicate via stdin/stdout (stdio transport, one JSON object per line).

Initial tool set (all stubs pending scene management):

| Tool | Description |
|------|-------------|
| `create_entity` | Add a named entity to the scene with type and world position |
| `delete_entity` | Remove an entity from the scene by name |
| `list_entities` | Return all entities in the current scene |
| `link_nodes` | Connect output/input pins in the visual script node graph |
| `bake_assets` | Trigger the asset compilation pipeline (mesh, texture, level) |
| `get_scene_state` | Return a full JSON snapshot of the scene state |

Built-in methods: `list_tools` / `rpc.discover` — enumerates all registered tool schemas for agent introspection.

---

## Build Instructions

### Prerequisites

- CMake 3.20 or later
- A C++17-capable host compiler (MSVC, GCC, or Clang)
- SDL2 and OpenGL development libraries (or let CMake FetchContent pull them automatically)
- For runtime cross-compilation: `mipsel-unknown-elf-gcc` on PATH (from PSn00bSDK or a manual GNU MIPS ELF toolchain build)

### Build the Editor (most common — daily development)

```bash
cmake -S . -B build/editor -DPSX_BUILD_TARGET=EDITOR
cmake --build build/editor
./build/editor/psx-editor/psx_editor
```

CMake will automatically download Dear ImGui (v1.91.5), SDL2 (v2.30.6), and nlohmann/json (v3.11.3) via FetchContent. No manual dependency installation required for the editor build.

### Cross-compile the PSX Runtime (requires MIPS toolchain)

```bash
cmake -S psx-runtime -B build/runtime \
  -DCMAKE_TOOLCHAIN_FILE=psx-runtime/cmake/mipsel-toolchain.cmake
cmake --build build/runtime
```

### Run Host Unit Tests (no cross-compiler needed)

```bash
cmake -S psx-runtime -B build/tests -DPSX_HOST_TEST=ON
cmake --build build/tests
ctest --test-dir build/tests -V
```

Tests cover: FP4_12/FP12_4 arithmetic, trigonometric LUT accuracy at known angles, Vec3 and Mat3 operations, scratchpad allocator behavior, heap alloc/free/coalesce, and MCP server JSON-RPC dispatch.

---

## Coding Standards

- No emojis anywhere in source files, headers, or strings.
- No `float` or `double` in any PSX runtime code. Enforced by the `-msoft-float` compiler flag.
- Every function, loop, bit-shift optimization, and complex structural decision must be commented with the reasoning, not just a description of what the code does.
- Every source file begins with a block comment describing its scope, purpose, dependencies, and execution context.
- All PSX runtime functions are named `PSX_` or module-prefixed (`Scratch_`, `Heap_`, `Vec3_`, etc.) to prevent accidental linkage against libc equivalents.

---

## Hardware Optimization Framework

Three memory-strain reduction strategies are built into the engine architecture:

**Procedural Geometry Generation** — Repetitive environment assets (paths, curves, tile floors) are calculated mathematically by the CPU using fixed-point equations at runtime rather than loading pre-built mesh arrays into RAM.

**Real-Time Bitstream Decompression** — Textures and level data are stored compressed in main RAM (LZ77/RLE variants). Decompression loops run on the CPU and stream data directly into VRAM via DMA immediately before frame rendering.

**Scratchpad RAM Utilization** — The 1 KB high-speed scratchpad at `0x1F800000` is used for intermediate transform matrices, sorting scratch buffers, and per-frame physics intermediates, bypassing the slower main memory bus entirely.

---

## Visual Illusion Techniques

**Hybrid 2D/3D Billboarding** — Distant 3D character models automatically swap to pre-rendered 2D sprites that always face the camera. Allows crowded environments without overloading the polygon budget.

**Dynamic Level-of-Detail** — Built-in code pathways throttle animation update frequency and reduce vertex counts based on camera distance.

**VRAM CLUT Sharing** — Multiple characters share localized Color Lookup Tables to maximize the 1 MB VRAM texture budget.

---

## License

This project is open-source. License file to be added.

---

## Contributing

This engine is built iteratively, module by module. Contributions must respect the hardware constraints and coding standards described in `.antigravity/instructions.md`. Do not add floating-point arithmetic to any code that runs on the PSX target. Do not create loose source files at the repository root — all code belongs in its respective module directory.
