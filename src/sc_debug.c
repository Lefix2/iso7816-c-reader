#include <stdbool.h>
#include <stdint.h>

#include "sc_debug.h"
#include "smartcard.h"

static sc_debug_hook_t g_sc_debug_hook       = NULL;
static uint8_t         g_sc_debug_categories = 0u;

void smartcard_Set_Debug_Hook(sc_debug_hook_t hook, uint8_t categories) {
  g_sc_debug_hook       = hook;
  g_sc_debug_categories = categories;
}

bool sc_dbg_enabled(uint8_t category) {
  return g_sc_debug_hook != NULL &&
         (g_sc_debug_categories & category) != 0u;
}

void sc_dbg(const char *tag, const uint8_t *data, uint32_t len) {
  if (g_sc_debug_hook && (g_sc_debug_categories & SC_DBG_CAT_GENERAL))
    g_sc_debug_hook(SC_DBG_CAT_GENERAL, tag, data, len);
}

void sc_dbg_atr(const char *tag, const uint8_t *data, uint32_t len) {
  if (g_sc_debug_hook && (g_sc_debug_categories & SC_DBG_CAT_ATR))
    g_sc_debug_hook(SC_DBG_CAT_ATR, tag, data, len);
}

void sc_dbg_pps(const char *tag, const uint8_t *data, uint32_t len) {
  if (g_sc_debug_hook && (g_sc_debug_categories & SC_DBG_CAT_PPS))
    g_sc_debug_hook(SC_DBG_CAT_PPS, tag, data, len);
}

void sc_dbg_apdu(const char *tag, const uint8_t *data, uint32_t len) {
  if (g_sc_debug_hook && (g_sc_debug_categories & SC_DBG_CAT_APDU))
    g_sc_debug_hook(SC_DBG_CAT_APDU, tag, data, len);
}

void sc_dbg_tpdu(const char *tag, const uint8_t *data, uint32_t len) {
  if (g_sc_debug_hook && (g_sc_debug_categories & SC_DBG_CAT_TPDU))
    g_sc_debug_hook(SC_DBG_CAT_TPDU, tag, data, len);
}
