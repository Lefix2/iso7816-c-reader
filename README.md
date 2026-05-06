# iso7816-c-reader

Portable C11 library implementing ISO/IEC 7816-3 smartcard communication — ATR parsing, PPS negotiation, T=0 and T=1 APDU/TPDU protocols.

Designed for embedded systems (no OS, no dynamic allocation), but equally usable on Linux/macOS/Windows.

## Features

- ATR parsing (direct and reverse convention, all interface bytes, TCK verification)
- PPS negotiation (protocol and baud rate selection)
- T=0: all APDU cases (1/2S/3S/4S/2E/3E/4E), GET RESPONSE chaining, wrong-length retry (0x6C)
- T=1: I/R/S-block state machine, chaining, IFS negotiation, WTX, EDC (LRC and CRC), resync with abort after 3 failures
- Hardware abstraction via `slot_itf_t` — implement once per platform
- Up to `SC_MAX_SLOTS` (2) simultaneous slots
- Zero dependencies beyond the C standard library

## Build

```sh
cmake -B build
cmake --build build
```

Both static (`libiso7816.a`) and shared (`libiso7816.so`) libraries are built.

### Run tests

```sh
cmake -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests use [Unity](https://github.com/ThrowTheSwitch/Unity) fetched automatically via CMake FetchContent.

### Options

| Variable | Default | Description |
|---|---|---|
| `BUILD_TESTING` | `ON` | Build and register the test suite |

## Integration

### As a CMake subdirectory

```cmake
add_subdirectory(iso7816-c-reader)
target_link_libraries(my_app PRIVATE iso7816::static)
# or iso7816::shared
```

### Manual

Add `include/` and `src/` to your compiler include path and compile the sources listed in `CMakeLists.txt` alongside your project.

## Porting

One thing to provide:

### `slot_itf_t` implementation

Implement the hardware abstraction for your UART/USART peripheral. See `test/slot_sim.c` for a reference (the simulation slot used by tests).

```c
slot_itf_t my_slot = {
    .init              = ...,
    .deinit            = ...,
    .activate          = ...,   /* power on, release RST */
    .deactivate        = ...,   /* power off */
    .send_byte         = ...,
    .send_bytes        = ...,
    .receive_byte      = ...,
    .receive_bytes     = ...,
    .set_frequency     = ...,
    .get_frequency     = ...,
    .set_timeout_etu   = ...,
    .get_timeout_etu   = ...,
    .set_guardtime_etu = ...,
    .get_guardtime_etu = ...,
    .set_convention    = ...,
    .get_convention    = ...,
    .set_F_D           = ...,
    .get_F_D           = ...,
    .set_IFSD          = ...,
    .get_IFSD          = ...,
    .get_min_etu_ns    = ...,   /* return sc_Status_Unsuported_feature if N/A */
};
```

## Usage

```c
#include "smartcard.h"

uint32_t slot;
uint8_t  atr[ATR_MAX_LENGTH], protocol;
uint32_t atr_len = sizeof(atr);

smartcard_Init();
smartcard_Register_slot(&my_slot, &slot);

sc_Status r = smartcard_Power_On(slot, SC_PROTOCOL_AUTO, atr, &atr_len, &protocol);

uint8_t cmd[]  = { 0x00, 0xA4, 0x04, 0x00, 0x00 };
uint8_t resp[256 + 2];
uint32_t resp_len = sizeof(resp);

r = smartcard_Xfer_Data(slot, cmd, sizeof(cmd), resp, &resp_len);

smartcard_Power_Off(slot);
```

## Repository layout

```
include/        Public headers (smartcard.h, slot_itf.h, sc_defs.h, sc_status.h, sc_debug.h)
src/
  smartcard.c   Public API implementation
  sc_defs.c     ISO 7816 parameter tables (Fi/Di/fmax)
  maths/        EDC — LRC and CRC-16
  protocols/    ATR, PPS, TPDU T=0, APDU T=0, TPDU T=1, APDU T=1
test/           Unity-based test suite + slot_sim (simulation slot)
```

## License

MIT — see [LICENSE](LICENSE).
