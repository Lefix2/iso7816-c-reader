#include "slot_sim.h"
#include "slot_itf.h"

static slot_sim_ctx_t g_ctx;

void slot_sim_setup(const uint8_t *rx,
                    uint32_t       rx_len,
                    uint8_t       *tx,
                    uint32_t       tx_cap) {
  g_ctx.rx_buf                    = rx;
  g_ctx.rx_len                    = rx_len;
  g_ctx.rx_pos                    = 0;
  g_ctx.tx_buf                    = tx;
  g_ctx.tx_cap                    = tx_cap;
  g_ctx.tx_pos                    = 0;
  g_ctx.present                   = true;
  g_ctx.powered                   = false;
  g_ctx.frequency                 = SC_fmaxd;
  g_ctx.F                         = SC_Fd;
  g_ctx.D                         = SC_Dd;
  g_ctx.timeout_etu               = SC_DEFAULT_WT_ETU;
  g_ctx.guardtime_etu             = 11;
  g_ctx.convention                = convention_direct;
  g_ctx.IFSD                      = ATR_DEFAULT_IFS;
  g_ctx.set_timeout_fail_countdown  = 0;
  g_ctx.set_guardtime_fail_countdown = 0;
  g_ctx.send_fail_countdown        = 0;
  g_ctx.receive_fail_countdown     = 0;
  g_ctx.get_ifsd_fail              = 0;
  g_ctx.get_min_etu_ns_result      = sc_Status_Unsuported_feature;
  g_ctx.get_min_etu_ns_value       = 0;
  g_ctx.init_fail_countdown        = 0;
  g_ctx.activate_fail              = 0;
  g_ctx.deactivate_fail            = 0;
  g_ctx.set_F_D_fail               = 0;
  g_ctx.set_frequency_fail         = 0;
}

slot_sim_ctx_t *slot_sim_get_ctx(void) { return &g_ctx; }

static sc_Status sim_init(void) {
  if (g_ctx.init_fail_countdown > 0) {
    if (--g_ctx.init_fail_countdown == 0)
      return sc_Status_Hardware_Error;
  }
  return sc_Status_Success;
}
static sc_Status sim_deinit(void) { return sc_Status_Success; }

static sc_Status sim_get_state(bool *present, bool *powered) {
  *present = g_ctx.present;
  *powered = g_ctx.powered;
  return sc_Status_Success;
}

static sc_Status sim_activate(sc_class_t class) {
  (void)class;
  if (g_ctx.activate_fail)
    return sc_Status_Hardware_Error;
  g_ctx.powered = true;
  return sc_Status_Success;
}

static sc_Status sim_deactivate(void) {
  if (g_ctx.deactivate_fail)
    return sc_Status_Hardware_Error;
  g_ctx.powered = false;
  return sc_Status_Success;
}

static sc_Status sim_send_byte(uint8_t byte) {
  if (g_ctx.send_fail_countdown > 0) {
    if (--g_ctx.send_fail_countdown == 0)
      return sc_Status_Hardware_Error;
  }
  if (g_ctx.tx_buf && g_ctx.tx_pos < g_ctx.tx_cap)
    g_ctx.tx_buf[g_ctx.tx_pos++] = byte;
  return sc_Status_Success;
}

static sc_Status sim_send_bytes(const uint8_t *ptr, uint32_t len) {
  if (g_ctx.send_fail_countdown > 0) {
    if (--g_ctx.send_fail_countdown == 0)
      return sc_Status_Hardware_Error;
  }
  for (uint32_t i = 0; i < len; i++) {
    if (g_ctx.tx_buf && g_ctx.tx_pos < g_ctx.tx_cap)
      g_ctx.tx_buf[g_ctx.tx_pos++] = ptr[i];
  }
  return sc_Status_Success;
}

static sc_Status sim_receive_byte(uint8_t *byte) {
  if (g_ctx.receive_fail_countdown > 0) {
    if (--g_ctx.receive_fail_countdown == 0)
      return sc_Status_Hardware_Error;
  }
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
  if (g_ctx.set_frequency_fail)
    return sc_Status_Hardware_Error;
  g_ctx.frequency = f;
  return sc_Status_Success;
}
static sc_Status sim_get_frequency(uint32_t *f) {
  *f = g_ctx.frequency;
  return sc_Status_Success;
}
static sc_Status sim_set_timeout_etu(uint32_t t) {
  if (g_ctx.set_timeout_fail_countdown > 0) {
    if (--g_ctx.set_timeout_fail_countdown == 0)
      return sc_Status_Hardware_Error;
  }
  g_ctx.timeout_etu = t;
  return sc_Status_Success;
}
static sc_Status sim_get_timeout_etu(uint32_t *t) {
  *t = g_ctx.timeout_etu;
  return sc_Status_Success;
}
static sc_Status sim_set_guardtime_etu(uint32_t g) {
  if (g_ctx.set_guardtime_fail_countdown > 0) {
    if (--g_ctx.set_guardtime_fail_countdown == 0)
      return sc_Status_Hardware_Error;
  }
  g_ctx.guardtime_etu = g;
  return sc_Status_Success;
}
static sc_Status sim_get_guardtime_etu(uint32_t *g) {
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
  if (g_ctx.set_F_D_fail)
    return sc_Status_Hardware_Error;
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
  if (g_ctx.get_ifsd_fail)
    return sc_Status_Hardware_Error;
  *ifsd = g_ctx.IFSD;
  return sc_Status_Success;
}

static sc_Status sim_get_min_etu_ns(uint32_t *etu_ns) {
  if (g_ctx.get_min_etu_ns_result == sc_Status_Success)
    *etu_ns = g_ctx.get_min_etu_ns_value;
  return g_ctx.get_min_etu_ns_result;
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
