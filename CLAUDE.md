# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Portable C11 library implementing ISO 7816-3 smartcard reader (ATR parsing, PPS negotiation, T=0 and T=1 APDU/TPDU). CMake build — static (`libiso7816.a`) and shared (`libiso7816.so`). Tests in `test/` use Unity via FetchContent.

## Build

```bash
cmake -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

CMake options:

| Variable | Default | Description |
|---|---|---|
| `BUILD_TESTING` | `ON` | Build test suite |
| `SC_MAX_SLOTS` | `2` | Maximum registered slots |

## Formatting

```bash
clang-format -i <file>
```

LLVM base style, see `.clang-format`. Key settings: `AlignConsecutiveDeclarations`, `AlignConsecutiveAssignments`, `AlignConsecutiveMacros`, `AllowShortEnumsOnASingleLine: false`, `BinPackParameters: false`.

## Architecture

Three abstraction layers communicate via vtable structs:

### 1. Public API (`include/smartcard.h`, `src/smartcard.c`)
Entry point for callers. Manages a static slot registry (`reg_p[SC_MAX_SLOTS]`). On `smartcard_Power_On`: activates the card class A→C, reads ATR, parses it into `iso_params_t`, performs PPS negotiation if card is in negotiable mode, then sets F/D/frequency on the slot.

Debug hook: `smartcard_Set_Debug_Hook(sc_debug_hook_t)` registers a runtime `(tag, data, len)` callback. Stored in `g_sc_debug_hook` (non-static, declared `extern` in `src/sc_debug.h`). Protocol files call `SC_DBG_COMM(tag, ptr, len)` — no-op when hook is NULL.

### 2. Protocol layer (`src/protocols/`)
Each protocol is a `protocol_itf_t` — a single `Transact(context, const uint8_t *send, slen, uint8_t *recv, rlen)` function pointer. Instances: `protocol_atr`, `protocol_pps`, `protocol_APDU_T0`, `protocol_TPDU_T0`, `protocol_APDU_T1`, `protocol_TPDU_T1`. APDU layers call TPDU layers internally. Public ATR accessors in `protocols.h` (`atr_get_*`) take `const atr_t *`.

### 3. Slot (hardware) layer (`include/slot_itf.h`)
`slot_itf_t` is the hardware abstraction: ~20 function pointers covering init/deinit, activate/deactivate, `send_bytes(const uint8_t*, len)`, receive, set/get frequency, F/D, guard time, timeout, convention, IFSD. Reference implementation: `test/slot_sim.c`.

### Context (`src/sc_context.h`)
`sc_context_t` = `slot_itf_t*` + `iso_params_t`. Passed through all protocol calls. `iso_params_t` (`include/sc_defs.h`) holds the full negotiated card state: ATR, F/D/fmax, protocol, T=0 WI, T=1 CWI/BWI/IFSC/IFSD/EDC, state machine enum.

## Porting to a new platform

1. Implement `slot_itf_t` for your hardware (use `test/slot_sim.c` as a minimal reference or `samples/` for real hardware examples).
2. Add `include/` and `src/` to your compiler include paths.
3. Compile the sources in `ISO7816_SOURCES` (see `CMakeLists.txt`) alongside your project.

No platform config header required — library uses `<string.h>` and `<stdint.h>` directly.

## Key constants (`include/sc_defs.h`)

- `SC_MAX_SLOTS` — max registered slots, default 2, override with `-DSC_MAX_SLOTS=N`
- Default F=372, D=1, fmax=4 MHz
- ATR max length: 46 bytes; historical bytes: 15
- T=1 max block: 261 bytes (3 prologue + 254 data + CRC)

## File layout

```
include/          Public headers (installed)
  smartcard.h     API + sc_debug_hook_t typedef
  slot_itf.h      Hardware abstraction vtable
  sc_defs.h       Types, constants, iso_params_t, atr_t
  sc_status.h     sc_Status enum
src/
  smartcard.c     API implementation, g_sc_debug_hook
  sc_defs.c       Fi/Di/fmax lookup tables
  sc_debug.h      Internal: SC_DBG_COMM macro + extern g_sc_debug_hook
  sc_context.h    sc_context_t definition
  maths/EDC.c     LRC and CRC-16
  protocols/      ATR, PPS, TPDU T=0/T=1, APDU T=0/T=1
samples/
  stm32wb_slot/   slot_itf_t implementation for STM32WB (HAL + DMA + CMSIS-OS2)
test/
  slot_sim.c/h    Software simulation slot
  test_atr.c      ATR parser tests (positive + negative)
  test_pps.c      PPS negotiation tests
  test_apdu_t0.c  T=0 APDU tests
  test_apdu_t1.c  T=1 APDU tests (chaining, WTX, resync, bad EDC)
.github/workflows/
  ci.yml          3 jobs: build-and-test, sanitizers (ASan+UBSan), coverage (lcov)
  release.yml     GitHub release on v* tag
```

## CI

- `build-and-test`: Debug build + ctest
- `sanitizers`: `-fsanitize=address,undefined`, `ASAN_OPTIONS=detect_leaks=1`
- `coverage`: `--coverage`, lcov/genhtml, artifact `coverage-report`
- `release`: `gh release create` with auto-generated notes, source archives only
