#ifndef SC_DEBUG_H_
#define SC_DEBUG_H_

#include "smartcard.h"

/* Defined (non-static) in smartcard.c */
extern sc_debug_hook_t g_sc_debug_hook;

#define SC_DBG_COMM(tag, ptr, len)                                    \
  do {                                                                 \
    if (g_sc_debug_hook)                                              \
      g_sc_debug_hook(tag, (const uint8_t *)(ptr), (uint32_t)(len)); \
  } while (0)

#endif /* SC_DEBUG_H_ */
