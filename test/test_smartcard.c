#include <string.h>

#include "unity.h"

#include "slot_sim.h"
#include "smartcard.h"

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static uint8_t  atr_buf[ATR_MAX_LENGTH];
static uint32_t atr_len;
static uint32_t g_slot;

/* Minimal T=0 ATR + PPS echo.
 * ATR: 3B 00 (no interface bytes)
 * PPS sent: FF 60 00 00 9F  (PPS0=0x60: T=0, PPS2+PPS3 present; PCK=0x9F)
 * Card echoes same bytes. */
static const uint8_t k_rx_t0[] = {0x3B, 0x00, 0xFF, 0x60, 0x00, 0x00, 0x9F};

/* T=1 ATR + PPS echo.
 * ATR: 3B 80 01 81 (TD1=T1, TCK=0x80^0x01=0x81)
 * PPS sent: FF 61 00 00 9E  (PPS0=0x61: T=1, PPS2+PPS3 present; PCK=0x9E)
 * Card echoes same bytes. */
static const uint8_t k_rx_t1[] = {0x3B, 0x80, 0x01, 0x81, 0xFF,
                                  0x61, 0x00, 0x00, 0x9E};

static sc_Status register_and_power_on_t0(void) {
  slot_sim_setup(k_rx_t0, sizeof(k_rx_t0), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  if (r != sc_Status_Success)
    return r;
  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  return smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len,
                            &protocol);
}

/* ── Debug hook tracking ─────────────────────────────────────────────────── */

static int  g_hook_calls;
static char g_hook_last_tag[32];

static void test_hook(const char *tag, const uint8_t *data, uint32_t len) {
  (void)data;
  (void)len;
  g_hook_calls++;
  strncpy(g_hook_last_tag, tag, sizeof(g_hook_last_tag) - 1);
}

/* ── Unity boilerplate ───────────────────────────────────────────────────── */

void setUp(void) {
  smartcard_Init();
  smartcard_Set_Debug_Hook(NULL);
  g_hook_calls       = 0;
  g_hook_last_tag[0] = '\0';
}

void tearDown(void) { smartcard_Set_Debug_Hook(NULL); }

/* ── Tests ───────────────────────────────────────────────────────────────── */

void test_sm_init(void) {
  sc_Status r = smartcard_Init();
  TEST_ASSERT_EQUAL(sc_Status_Success, r);
}

void test_sm_register_and_unregister(void) {
  slot_sim_setup(NULL, 0, NULL, 0);
  uint32_t  slot;
  sc_Status r = smartcard_Register_slot(&hslot_sim, &slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(0, slot);

  r = smartcard_UnRegister_slot(slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);
}

void test_sm_register_null_slot_number(void) {
  sc_Status r = smartcard_Register_slot(&hslot_sim, NULL);
  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, r);
}

void test_sm_register_overflow(void) {
  slot_sim_setup(NULL, 0, NULL, 0);
  uint32_t s0, s1, s2;
  TEST_ASSERT_EQUAL(sc_Status_Success,
                    smartcard_Register_slot(&hslot_sim, &s0));
  TEST_ASSERT_EQUAL(sc_Status_Success,
                    smartcard_Register_slot(&hslot_sim, &s1));
  sc_Status r = smartcard_Register_slot(&hslot_sim, &s2);
  TEST_ASSERT_EQUAL(sc_Status_Buffer_To_Small, r);
}

void test_sm_unregister_bad_slot(void) {
  sc_Status r = smartcard_UnRegister_slot(99);
  TEST_ASSERT_EQUAL(sc_Status_Bad_Slot, r);
}

void test_sm_power_on_bad_slot(void) {
  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  sc_Status r =
      smartcard_Power_On(99, SC_PROTOCOL_AUTO, atr_buf, &atr_len, &protocol);
  TEST_ASSERT_EQUAL(sc_Status_Bad_Slot, r);
}

void test_sm_power_off_bad_slot(void) {
  sc_Status r = smartcard_Power_Off(99);
  TEST_ASSERT_EQUAL(sc_Status_Bad_Slot, r);
}

void test_sm_power_on_t0(void) {
  slot_sim_setup(k_rx_t0, sizeof(k_rx_t0), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r       = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len,
                               &protocol);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, atr_len);
  TEST_ASSERT_EQUAL(SC_PROTOCOL_T0, protocol);
  TEST_ASSERT_EQUAL_HEX8(0x3B, atr_buf[0]);
}

void test_sm_power_on_t1(void) {
  slot_sim_setup(k_rx_t1, sizeof(k_rx_t1), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r       = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len,
                               &protocol);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(SC_PROTOCOL_T1, protocol);
}

void test_sm_power_off(void) {
  sc_Status r = register_and_power_on_t0();
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  r = smartcard_Power_Off(g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_FALSE(smartcard_Is_Powered(g_slot));
}

void test_sm_is_present_and_powered(void) {
  sc_Status r = register_and_power_on_t0();
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  TEST_ASSERT_TRUE(smartcard_Is_Present(g_slot));
  TEST_ASSERT_TRUE(smartcard_Is_Powered(g_slot));
}

void test_sm_is_present_bad_slot(void) {
  TEST_ASSERT_FALSE(smartcard_Is_Present(99));
}

void test_sm_is_powered_bad_slot(void) {
  TEST_ASSERT_FALSE(smartcard_Is_Powered(99));
}

void test_sm_xfer_bad_state(void) {
  slot_sim_setup(NULL, 0, NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t  cmd[] = {0x00, 0x20, 0x00, 0x00};
  uint8_t  resp[8];
  uint32_t resp_len = sizeof(resp);
  r = smartcard_Xfer_Data(g_slot, cmd, sizeof(cmd), resp, &resp_len);
  TEST_ASSERT_EQUAL(sc_Status_Bad_State, r);
  TEST_ASSERT_EQUAL(0, resp_len);
}

void test_sm_xfer_bad_slot(void) {
  uint8_t   cmd[] = {0x00, 0x20, 0x00, 0x00};
  uint8_t   resp[8];
  uint32_t  resp_len = sizeof(resp);
  sc_Status r = smartcard_Xfer_Data(99, cmd, sizeof(cmd), resp, &resp_len);
  TEST_ASSERT_EQUAL(sc_Status_Bad_Slot, r);
}

void test_sm_xfer_t0_case1(void) {
  sc_Status r = register_and_power_on_t0();
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  /* Case 1 APDU: 4 bytes, no data in/out, card returns SW directly */
  static const uint8_t apdu_rx[] = {0x90, 0x00};
  uint8_t              tx_cap[32];
  slot_sim_setup(apdu_rx, sizeof(apdu_rx), tx_cap, sizeof(tx_cap));

  uint8_t  cmd[] = {0x00, 0x20, 0x00, 0x00};
  uint8_t  resp[8];
  uint32_t resp_len = sizeof(resp);
  r = smartcard_Xfer_Data(g_slot, cmd, sizeof(cmd), resp, &resp_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, resp_len);
  TEST_ASSERT_EQUAL_HEX8(0x90, resp[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, resp[1]);
}

void test_sm_power_on_with_pps1(void) {
  /* ATR: 3B 10 97 — TA1=0x97: Fi=9=512, Di=7=64 → F/D=8 < 372 → PPS1 included
   * PPS: FF 70 97 00 00 18  (PPS0=0x70: T=0+PPS1+PPS2+PPS3, PPS1=0x97,
   * PCK=0x18) PCK: 0xFF^0x70^0x97^0x00^0x00 = 0x18 */
  static const uint8_t rx[] = {0x3B, 0x10, 0x97, /* ATR */
                               0xFF, 0x70, 0x97,
                               0x00, 0x00, 0x18}; /* PPS echo */
  slot_sim_setup(rx, sizeof(rx), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r       = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len,
                               &protocol);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(SC_PROTOCOL_T0, protocol);
}

void test_sm_xfer_t1_case1(void) {
  /* Power on with T=1 ATR */
  slot_sim_setup(k_rx_t1, sizeof(k_rx_t1), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r       = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len,
                               &protocol);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(SC_PROTOCOL_T1, protocol);

  /* T=1 exchange: IFS negotiation + 4-byte Case 1 APDU
   * IFS response from card: NAD=00 PCB=E1 LEN=01 IFSD=20 LRC=C0
   * I-block response:       NAD=00 PCB=00 LEN=02 90 00 LRC=92 */
  static const uint8_t t1_rx[] = {
      0x00, 0xE1, 0x01, 0x20, 0xC0,      /* IFS response */
      0x00, 0x00, 0x02, 0x90, 0x00, 0x92 /* I-block response */
  };
  uint8_t tx_cap[64];
  slot_sim_setup(t1_rx, sizeof(t1_rx), tx_cap, sizeof(tx_cap));

  uint8_t  cmd[] = {0x00, 0x20, 0x00, 0x00};
  uint8_t  resp[16];
  uint32_t resp_len = sizeof(resp);
  r = smartcard_Xfer_Data(g_slot, cmd, sizeof(cmd), resp, &resp_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, resp_len);
  TEST_ASSERT_EQUAL_HEX8(0x90, resp[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, resp[1]);
}

void test_sm_power_on_ta2_specific_t0(void) {
  /* ATR: 3B 80 10 00
   * T0=0x80: TD1 present; TD1=0x10: TA2 present, T=0; TA2=0x00: specific T=0
   * No PPS exchange in specific mode; F=Fi, D=Di applied directly */
  static const uint8_t rx[] = {0x3B, 0x80, 0x10, 0x00};
  slot_sim_setup(rx, sizeof(rx), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len, &protocol);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(SC_PROTOCOL_T0, protocol);
}

void test_sm_power_on_ta2_b5_set(void) {
  /* ATR: 3B 80 10 10 — TA2=0x10: b5 set (implicit F/D), unsupported */
  static const uint8_t rx[] = {0x3B, 0x80, 0x10, 0x10};
  slot_sim_setup(rx, sizeof(rx), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len, &protocol);

  TEST_ASSERT_EQUAL(sc_Status_Unsuported_feature, r);
}

void test_sm_power_on_preferred_t0(void) {
  /* Same ATR+PPS as k_rx_t0 but preferred_protocol=T0; exercises the
   * preferred-protocol override branch (line 447 in smartcard.c) */
  slot_sim_setup(k_rx_t0, sizeof(k_rx_t0), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r = smartcard_Power_On(g_slot, SC_PROTOCOL_T0, atr_buf, &atr_len, &protocol);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(SC_PROTOCOL_T0, protocol);
}

void test_sm_power_on_atr_fail(void) {
  /* Empty sim: activate succeeds but ATR receive times out.
   * Library retries class_B, class_C; all fail → deactivate each time,
   * then returns the timeout error. */
  slot_sim_setup(NULL, 0, NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len, &protocol);

  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

void test_sm_pps_transact_timeout(void) {
  /* Only ATR bytes in sim; PPS receive times out → Power_On returns
   * PPS_Unsuccessfull (exercises the protocol_pps.Transact failure path) */
  static const uint8_t rx[] = {0x3B, 0x00};
  slot_sim_setup(rx, sizeof(rx), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len, &protocol);

  TEST_ASSERT_EQUAL(sc_Status_PPS_Unsuccessfull, r);
}

void test_sm_finalize_pps_protocol_mismatch(void) {
  /* ATR=3B 00 (T=0). We send PPS0=0x60 (T=0+PPS2+PPS3, len=5).
   * Card echoes PPS0=0x61 (T=1+PPS2, len=4) — different length so
   * protocol_pps passes; finalize_pps detects T nibble mismatch (rule 1). */
  static const uint8_t rx[] = {0x3B, 0x00, 0xFF, 0x21, 0x00, 0xDE};
  slot_sim_setup(rx, sizeof(rx), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len, &protocol);

  TEST_ASSERT_EQUAL(sc_Status_PPS_Unsuccessfull, r);
}

void test_sm_finalize_pps_pps1_unexpected(void) {
  /* ATR=3B 00: no TA1 → we don't send PPS1 (PPS0=0x60).
   * Card adds PPS1 in response (PPS0=0x70, len=6 vs our 5).
   * finalize_pps rule 2: PPS1 present in response but not in request. */
  static const uint8_t rx[] = {0x3B, 0x00, 0xFF, 0x70, 0x97, 0x00, 0x00, 0x18};
  slot_sim_setup(rx, sizeof(rx), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len, &protocol);

  TEST_ASSERT_EQUAL(sc_Status_PPS_Unsuccessfull, r);
}

void test_sm_finalize_pps_pps1_mismatch(void) {
  /* ATR=3B 10 97: TA1=0x97 → we send PPS1=0x97 (PPS0=0x70, len=6).
   * Card responds with PPS1=0x11 and drops PPS3 (PPS0=0x30, len=5).
   * finalize_pps rule 2: PPS1 value differs (0x97 vs 0x11). */
  static const uint8_t rx[] = {0x3B, 0x10, 0x97, 0xFF, 0x30, 0x11, 0x00, 0xDE};
  slot_sim_setup(rx, sizeof(rx), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len, &protocol);

  TEST_ASSERT_EQUAL(sc_Status_PPS_Unsuccessfull, r);
}

void test_sm_finalize_pps_pps2_mismatch(void) {
  /* ATR=3B 00: we send PPS2=0x00 (PPS0=0x60, len=5).
   * Card responds with PPS2=0x01 and drops PPS3 (PPS0=0x20, len=4).
   * finalize_pps rule 3: PPS2 value differs (0x00 vs 0x01). */
  static const uint8_t rx[] = {0x3B, 0x00, 0xFF, 0x20, 0x01, 0xDE};
  slot_sim_setup(rx, sizeof(rx), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len, &protocol);

  TEST_ASSERT_EQUAL(sc_Status_PPS_Unsuccessfull, r);
}

void test_sm_finalize_pps_pps3_mismatch(void) {
  /* ATR=3B 00: we send PPS3=0x00 (PPS0=0x60, len=5).
   * Card drops PPS2 and sends PPS3=0x01 (PPS0=0x40, len=4).
   * finalize_pps rule 4: PPS3 value differs (0x00 vs 0x01). */
  static const uint8_t rx[] = {0x3B, 0x00, 0xFF, 0x40, 0x01, 0xBE};
  slot_sim_setup(rx, sizeof(rx), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len, &protocol);

  TEST_ASSERT_EQUAL(sc_Status_PPS_Unsuccessfull, r);
}

void test_sm_debug_hook(void) {
  smartcard_Set_Debug_Hook(test_hook);
  slot_sim_setup(k_rx_t0, sizeof(k_rx_t0), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r       = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len,
                               &protocol);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  /* Hook fired at least for ATR, PPS, power_on events */
  TEST_ASSERT_GREATER_THAN(0, g_hook_calls);
}

/* ── initfromatr failure paths ───────────────────────────────────────────── */

/* initfromatr_global → atr_get_WI: TC2=0x00 is forbidden (ISO 7816-3 §8.3).
 * ATR: TS T0=0x80(TD1) TD1=0x40(TC2) TC2=0x00 — no TCK (T=0 only). */
void test_sm_initfromatr_wi_zero(void) {
  static const uint8_t rx[] = {0x3B, 0x80, 0x40, 0x00};
  slot_sim_setup(rx, sizeof(rx), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len, &protocol);

  TEST_ASSERT_EQUAL(sc_Status_ATR_Malformed, r);
}

/* initfromatr_global → atr_get_fmax / atr_get_Fi: Fi index 7 maps to
 * f_table[7]=0 (reserved). Both fmax and Fi use the same iFi=TA1>>4; fmax
 * is checked first so it fails first, making atr_get_Fi unreachable.
 * ATR: TS T0=0x10(TA1) TA1=0x71(Fi=7,Di=1) — no TCK. */
void test_sm_initfromatr_fi_reserved(void) {
  static const uint8_t rx[] = {0x3B, 0x10, 0x71};
  slot_sim_setup(rx, sizeof(rx), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len, &protocol);

  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, r);
}

/* initfromatr_global → atr_get_Di: Di index 0 maps to d_table[0]=0
 * (reserved). Fi=1 (TA1>>4=1, f_table[1]=372) is valid so fmax and Fi
 * succeed; atr_get_Di is then reached and fails.
 * ATR: TS T0=0x10(TA1) TA1=0x10(Fi=1,Di=0) — no TCK. */
void test_sm_initfromatr_di_reserved(void) {
  static const uint8_t rx[] = {0x3B, 0x10, 0x10};
  slot_sim_setup(rx, sizeof(rx), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len, &protocol);

  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, r);
}

/* initfromatr_T1_specific → atr_T1_specific_get_IFS: TA3=0x00 is forbidden.
 * ATR: TS T0=0x80(TD1) TD1=0x81(TD2,T1) TD2=0x11(TA3,T1) TA3=0x00 TCK=0x10
 * TCK = T0^TD1^TD2^TA3 = 0x80^0x81^0x11^0x00 = 0x10. */
void test_sm_initfromatr_ifs_zero(void) {
  static const uint8_t rx[] = {0x3B, 0x80, 0x81, 0x11, 0x00, 0x10};
  slot_sim_setup(rx, sizeof(rx), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len, &protocol);

  TEST_ASSERT_EQUAL(sc_Status_ATR_Malformed, r);
}

/* initfromatr_T1_specific → atr_T1_specific_get_CBWI: BWI=(TB3>>4)=0xA=10>9.
 * ATR: TS T0=0x80(TD1) TD1=0x81(TD2,T1) TD2=0x21(TB3,T1) TB3=0xA5 TCK=0x85
 * TCK = 0x80^0x81^0x21^0xA5 = 0x85. */
void test_sm_initfromatr_bwi_reserved(void) {
  static const uint8_t rx[] = {0x3B, 0x80, 0x81, 0x21, 0xA5, 0x85};
  slot_sim_setup(rx, sizeof(rx), NULL, 0);
  sc_Status r = smartcard_Register_slot(&hslot_sim, &g_slot);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);

  uint8_t protocol;
  atr_len = sizeof(atr_buf);
  r = smartcard_Power_On(g_slot, SC_PROTOCOL_AUTO, atr_buf, &atr_len, &protocol);

  TEST_ASSERT_EQUAL(sc_Status_ATR_Malformed, r);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_sm_init);
  RUN_TEST(test_sm_register_and_unregister);
  RUN_TEST(test_sm_register_null_slot_number);
  RUN_TEST(test_sm_register_overflow);
  RUN_TEST(test_sm_unregister_bad_slot);
  RUN_TEST(test_sm_power_on_bad_slot);
  RUN_TEST(test_sm_power_off_bad_slot);
  RUN_TEST(test_sm_power_on_t0);
  RUN_TEST(test_sm_power_on_t1);
  RUN_TEST(test_sm_power_off);
  RUN_TEST(test_sm_is_present_and_powered);
  RUN_TEST(test_sm_is_present_bad_slot);
  RUN_TEST(test_sm_is_powered_bad_slot);
  RUN_TEST(test_sm_xfer_bad_state);
  RUN_TEST(test_sm_xfer_bad_slot);
  RUN_TEST(test_sm_xfer_t0_case1);
  RUN_TEST(test_sm_power_on_with_pps1);
  RUN_TEST(test_sm_xfer_t1_case1);
  RUN_TEST(test_sm_debug_hook);
  RUN_TEST(test_sm_power_on_ta2_specific_t0);
  RUN_TEST(test_sm_power_on_ta2_b5_set);
  RUN_TEST(test_sm_power_on_preferred_t0);
  RUN_TEST(test_sm_power_on_atr_fail);
  RUN_TEST(test_sm_pps_transact_timeout);
  RUN_TEST(test_sm_finalize_pps_protocol_mismatch);
  RUN_TEST(test_sm_finalize_pps_pps1_unexpected);
  RUN_TEST(test_sm_finalize_pps_pps1_mismatch);
  RUN_TEST(test_sm_finalize_pps_pps2_mismatch);
  RUN_TEST(test_sm_finalize_pps_pps3_mismatch);
  /* initfromatr failure paths */
  RUN_TEST(test_sm_initfromatr_wi_zero);
  RUN_TEST(test_sm_initfromatr_fi_reserved);
  RUN_TEST(test_sm_initfromatr_di_reserved);
  RUN_TEST(test_sm_initfromatr_ifs_zero);
  RUN_TEST(test_sm_initfromatr_bwi_reserved);
  return UNITY_END();
}
