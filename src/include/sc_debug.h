#ifndef SC_DEBUG_H_
#define SC_DEBUG_H_

#include <stdbool.h>
#include <stdint.h>

/* Implemented in src/sc_debug.c */
bool sc_dbg_enabled(uint8_t category);
void sc_dbg(const char *tag, const uint8_t *data, uint32_t len);
void sc_dbg_atr(const char *tag, const uint8_t *data, uint32_t len);
void sc_dbg_pps(const char *tag, const uint8_t *data, uint32_t len);
void sc_dbg_apdu(const char *tag, const uint8_t *data, uint32_t len);
void sc_dbg_tpdu(const char *tag, const uint8_t *data, uint32_t len);

#endif /* SC_DEBUG_H_ */
