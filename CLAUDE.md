# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Portable C library implementing ISO 7816 smartcard reader (ATR parsing, PPS negotiation, T=0 and T=1 APDU/TPDU). No build system is included — the library is integrated into a host project. The `example/FreeRTOS/` directory is a complete STM32L476 + FreeRTOS integration using STM32CubeIDE.

## Formatting

```bash
clang-format -i <file>   # LLVM style, see .clang-format
```

## Architecture

Three abstraction layers communicate via vtable structs:

### 1. Public API (`include/smartcard.h`, `src/smartcard.c`)
Entry point for callers. Manages a static slot registry (`reg_p[SC_MAX_SLOTS]`). On `smartcard_Power_On`: activates the card class A→C, reads ATR, parses it into `iso_params_t`, performs PPS negotiation if card is in negotiable mode, then sets F/D/frequency on the slot.

### 2. Protocol layer (`src/protocols/`)
Each protocol is a `protocol_itf_t` — a single `Transact(context, send, slen, recv, rlen)` function pointer. Instances: `protocol_atr`, `protocol_pps`, `protocol_APDU_T0`, `protocol_TPDU_T0`, `protocol_APDU_T1`, `protocol_TPDU_T1`. APDU layers call TPDU layers internally.

### 3. Slot (hardware) layer (`include/slot_itf.h`)
`slot_itf_t` is the hardware abstraction: ~20 function pointers covering init/deinit, activate/deactivate, send/receive bytes, set/get frequency, F/D, guard time, timeout, convention, IFSD. The provided implementation (`src/slots/slot_template.c`) uses STM32 HAL SMARTCARD peripheral + FreeRTOS semaphore.

### Context (`src/sc_context.h`)
`sc_context_t` = `slot_itf_t*` + `iso_params_t`. Passed through all protocol calls. `iso_params_t` (`include/sc_defs.h`) holds the full negotiated card state: ATR, F/D/fmax, protocol, T=0 WI, T=1 CWI/BWI/IFSC/IFSD/EDC, state machine enum.

## Porting to a new platform

1. Implement `slot_itf_t` for your hardware (use `src/slots/slot_template.c` as reference).
2. Provide `smartcard_config.h` — maps `memset`/`memcpy`/etc. and controls `ENABLE_DEBUG_ISO7816`.
3. Provide `type.h` (basic types) and `debug.h` (`dbg_info`, `dbg_comm` macros).
4. Add `include/` and `src/` to your compiler include paths.

## Key constants (`include/sc_defs.h`)

- `SC_MAX_SLOTS 2` — maximum registered slots
- Default F=372, D=1, fmax=4 MHz
- ATR max length: 46 bytes; historical bytes: 15
- T=1 max block: 261 bytes (3 prologue + 254 data + CRC)
