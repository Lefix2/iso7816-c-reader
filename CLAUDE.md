# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Portable C11 library implementing ISO 7816 smartcard reader (ATR parsing, PPS negotiation, T=0 and T=1 APDU/TPDU). Built with CMake — produces static and shared libraries. Tests in `test/` use Unity via FetchContent.

## Formatting

```bash
clang-format -i <file>   # LLVM style, see .clang-format
```

## Architecture

Three abstraction layers communicate via vtable structs:

### 1. Public API (`include/smartcard.h`, `src/smartcard.c`)
Entry point for callers. Manages a static slot registry (`reg_p[SC_MAX_SLOTS]`). On `smartcard_Power_On`: activates the card class A→C, reads ATR, parses it into `iso_params_t`, performs PPS negotiation if card is in negotiable mode, then sets F/D/frequency on the slot. `smartcard_Set_Debug_Hook()` registers a runtime callback `(tag, data, len)` for protocol tracing — stored in `g_sc_debug_hook` (non-static, declared extern in `src/sc_debug.h`).

### 2. Protocol layer (`src/protocols/`)
Each protocol is a `protocol_itf_t` — a single `Transact(context, send, slen, recv, rlen)` function pointer. Instances: `protocol_atr`, `protocol_pps`, `protocol_APDU_T0`, `protocol_TPDU_T0`, `protocol_APDU_T1`, `protocol_TPDU_T1`. APDU layers call TPDU layers internally.

### 3. Slot (hardware) layer (`include/slot_itf.h`)
`slot_itf_t` is the hardware abstraction: ~20 function pointers covering init/deinit, activate/deactivate, send/receive bytes, set/get frequency, F/D, guard time, timeout, convention, IFSD. Reference implementation: `test/slot_sim.c` (software simulation used by tests).

### Context (`src/sc_context.h`)
`sc_context_t` = `slot_itf_t*` + `iso_params_t`. Passed through all protocol calls. `iso_params_t` (`include/sc_defs.h`) holds the full negotiated card state: ATR, F/D/fmax, protocol, T=0 WI, T=1 CWI/BWI/IFSC/IFSD/EDC, state machine enum.

## Porting to a new platform

1. Implement `slot_itf_t` for your hardware (use `test/slot_sim.c` as reference).
2. Add `include/` and `src/` to your compiler include paths.
3. Compile sources listed in `CMakeLists.txt` (`ISO7816_SOURCES`) alongside your project.

## Key constants (`include/sc_defs.h`)

- `SC_MAX_SLOTS` — max registered slots, default 2, override with `-DSC_MAX_SLOTS=N`
- Default F=372, D=1, fmax=4 MHz
- ATR max length: 46 bytes; historical bytes: 15
- T=1 max block: 261 bytes (3 prologue + 254 data + CRC)

## CI

Three jobs: `build-and-test` (plain Debug), `sanitizers` (ASan+UBSan), `coverage` (lcov artifact). Release on `v*` tag via `release.yml`.
