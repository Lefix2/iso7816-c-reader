#include "slot_sim.h"
#include "sc_defs.h"
#include "slot_itf.h"

static slot_sim_ctx_t g_ctx;

void slot_sim_setup(const uint8_t *rx,
                    uint32_t       rx_len,
                    uint8_t       *tx,
                    uint32_t       tx_cap) {
  g_ctx.rx_buf        = rx;
  g_ctx.rx_len        = rx_len;
  g_ctx.rx_pos        = 0;
  g_ctx.tx_buf        = tx;
  g_ctx.tx_cap        = tx_cap;
  g_ctx.tx_pos        = 0;
  g_ctx.present       = true;
  g_ctx.powered       = false;
  g_ctx.frequency     = SC_fmaxd;
  g_ctx.F             = SC_Fd;
  g_ctx.D             = SC_Dd;
  g_ctx.timeout_etu   = SC_DEFAULT_WT_ETU;
  g_ctx.guardtime_etu = 11;
  g_ctx.convention    = convention_direct;
  g_ctx.IFSD          = ATR_DEFAULT_IFS;
}

slot_sim_ctx_t *slot_sim_get_ctx(void) { return &g_ctx; }

static sc_Status sim_init(void) { return sc_Status_Success; }
static sc_Status sim_deinit(void) { return sc_Status_Success; }

static sc_Status sim_get_state(bool *present, bool *powered) {
  *present = g_ctx.present;
  *powered = g_ctx.powered;
  return sc_Status_Success;
}

static sc_Status sim_activate(sc_class_t class) {
  (void)class;
  g_ctx.powered = true;
  return sc_Status_Success;
}

static sc_Status sim_deactivate(void) {
  g_ctx.powered = false;
  return sc_Status_Success;
}

static sc_Status sim_send_byte(uint8_t byte) {
  if (g_ctx.tx_buf && g_ctx.tx_pos < g_ctx.tx_cap)
    g_ctx.tx_buf[g_ctx.tx_pos++] = byte;
  return sc_Status_Success;
}

static sc_Status sim_send_bytes(uint8_t *ptr, uint32_t len) {
  for (uint32_t i = 0; i < len; i++)
    sim_send_byte(ptr[i]);
  return sc_Status_Success;
}

static sc_Status sim_receive_byte(uint8_t *byte) {
  if (g_ctx.rx_pos >= g_ctx.rx_len)
    return sc_Status_Slot_Reception_Timeout;
  *byte = g_ctx.rx_buf[g_ctx.rx_pos++];
  return sc_Status_Success;
}

static sc_Status sim_receive_bytes(uint8_t *ptr, uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    sc_Status s = sim_receive_byte(&ptr[i]);
    if (s != sc_Status_Success)
      return s;
  }
  return sc_Status_Success;
}

static sc_Status sim_set_frequency(uint32_t f) {
  g_ctx.frequency = f;
  return sc_Status_Success;
}
static sc_Status sim_get_frequency(uint32_t *f) {
  *f = g_ctx.frequency;
  return sc_Status_Success;
}
static sc_Status sim_set_timeout_etu(uint32_t t) {
  g_ctx.timeout_etu = t;
  return sc_Status_Success;
}
static sc_Status sim_get_timeout_etu(uint32_t *t) {
  *t = g_ctx.timeout_etu;
  return sc_Status_Success;
}
static sc_Status sim_set_guardtime_etu(uint8_t g) {
  g_ctx.guardtime_etu = g;
  return sc_Status_Success;
}
static sc_Status sim_get_guardtime_etu(uint8_t *g) {
  *g = g_ctx.guardtime_etu;
  return sc_Status_Success;
}
static sc_Status sim_set_convention(sc_convention_t c) {
  g_ctx.convention = c;
  return sc_Status_Success;
}
static sc_Status sim_get_convention(sc_convention_t *c) {
  *c = g_ctx.convention;
  return sc_Status_Success;
}
static sc_Status sim_set_F_D(uint32_t F, uint32_t D) {
  g_ctx.F = F;
  g_ctx.D = D;
  return sc_Status_Success;
}
static sc_Status sim_get_F_D(uint32_t *F, uint32_t *D) {
  *F = g_ctx.F;
  *D = g_ctx.D;
  return sc_Status_Success;
}
static sc_Status sim_set_IFSD(uint8_t ifsd) {
  g_ctx.IFSD = ifsd;
  return sc_Status_Success;
}
static sc_Status sim_get_IFSD(uint8_t *ifsd) {
  *ifsd = g_ctx.IFSD;
  return sc_Status_Success;
}

/* Return sc_Status_Unsuported_feature so caller uses its own min-etu logic */
static sc_Status sim_get_min_etu_ns(uint32_t *etu_ns) {
  (void)etu_ns;
  return sc_Status_Unsuported_feature;
}

slot_itf_t hslot_sim = {
    sim_init,
    sim_deinit,
    sim_get_state,
    sim_activate,
    sim_deactivate,
    sim_send_byte,
    sim_send_bytes,
    sim_receive_byte,
    sim_receive_bytes,
    sim_set_frequency,
    sim_get_frequency,
    sim_set_timeout_etu,
    sim_get_timeout_etu,
    sim_set_guardtime_etu,
    sim_get_guardtime_etu,
    sim_set_convention,
    sim_get_convention,
    sim_set_F_D,
    sim_get_F_D,
    sim_set_IFSD,
    sim_get_IFSD,
    sim_get_min_etu_ns,
};
