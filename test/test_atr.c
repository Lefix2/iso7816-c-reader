#include "unity.h"

#include "protocols/protocols.h"
#include "sc_context.h"
#include "sc_defs.h"
#include "slot_sim.h"

static sc_context_t ctx;
static uint8_t      atr_buf[ATR_MAX_LENGTH];
static uint32_t     atr_len;

static void setup_context(void) {
  iso_params_init(&ctx.params);
  ctx.params.state = sc_state_reset_high;
  ctx.slot         = &hslot_sim;
}

void setUp(void) {}
void tearDown(void) {}

/* ── ATR: minimal T=0, no interface bytes, no historical ─────────────────── */
void test_atr_minimal_t0(void) {
  static const uint8_t raw[] = {0x3B, 0x00};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, atr_len);
  TEST_ASSERT_EQUAL_HEX8(0x3B, ctx.params.ATR.TS);
  TEST_ASSERT_EQUAL_HEX8(0x00, ctx.params.ATR.T0);
  TEST_ASSERT_EQUAL(0, ctx.params.ATR.nb_HB);
  TEST_ASSERT_FALSE(ctx.params.ATR.TCK.present);
}

/* ── ATR: TA1 present (Fi=1=372, Di=1=1) ────────────────────────────────── */
void test_atr_with_ta1(void) {
  /* T0=0x10: TA1 present, K=0 historical */
  static const uint8_t raw[] = {0x3B, 0x10, 0x11};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(3, atr_len);
  TEST_ASSERT_TRUE(ctx.params.ATR.T[0][ATR_INTERFACE_A].present);
  TEST_ASSERT_EQUAL_HEX8(0x11, ctx.params.ATR.T[0][ATR_INTERFACE_A].value);
}

/* ── ATR: historical bytes ───────────────────────────────────────────────── */
void test_atr_historical_bytes(void) {
  /* T0=0x03: K=3 historical bytes, no interface bytes */
  static const uint8_t raw[] = {0x3B, 0x03, 0xAA, 0xBB, 0xCC};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(5, atr_len);
  TEST_ASSERT_EQUAL(3, ctx.params.ATR.nb_HB);
  TEST_ASSERT_EQUAL_HEX8(0xAA, ctx.params.ATR.HB[0]);
  TEST_ASSERT_EQUAL_HEX8(0xBB, ctx.params.ATR.HB[1]);
  TEST_ASSERT_EQUAL_HEX8(0xCC, ctx.params.ATR.HB[2]);
}

/* ── ATR: T=1 with TCK ───────────────────────────────────────────────────── */
void test_atr_t1_with_tck(void) {
  /* T0=0x80: TD1 present, K=0
   * TD1=0x01: T=1, no more interface bytes → TCK required
   * TCK = T0 XOR TD1 = 0x80 XOR 0x01 = 0x81
   */
  static const uint8_t raw[] = {0x3B, 0x80, 0x01, 0x81};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(4, atr_len);
  TEST_ASSERT_TRUE(ctx.params.ATR.T[0][ATR_INTERFACE_D].present);
  TEST_ASSERT_EQUAL_HEX8(0x01, ctx.params.ATR.T[0][ATR_INTERFACE_D].value);
  TEST_ASSERT_TRUE(ctx.params.ATR.TCK.present);
  TEST_ASSERT_EQUAL_HEX8(0x81, ctx.params.ATR.TCK.value);
}

/* ── ATR: reverse convention (TS=0x03 → 0x3F) ───────────────────────────── */
void test_atr_reverse_convention(void) {
  /* 0x03 is 0x3F in reverse convention — lib auto-converts */
  static const uint8_t raw[] = {0x03, 0x00};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL_HEX8(0x3F, atr_buf[0]);
  TEST_ASSERT_EQUAL_HEX8(0x3F, ctx.params.ATR.TS);
}

/* ── ATR: bad TS ─────────────────────────────────────────────────────────── */
void test_atr_bad_ts(void) {
  static const uint8_t raw[] = {0x11, 0x00};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);

  TEST_ASSERT_EQUAL(sc_Status_ATR_Bad_TS, r);
}

/* ── ATR: buffer too small ───────────────────────────────────────────────── */
void test_atr_buffer_too_small(void) {
  static const uint8_t raw[] = {0x3B, 0x00};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = 1; /* need at least 2 */

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);

  TEST_ASSERT_EQUAL(sc_Status_Buffer_To_Small, r);
}

/* ── ATR: bad state (not reset_high) ─────────────────────────────────────── */
void test_atr_bad_state(void) {
  static const uint8_t raw[] = {0x3B, 0x00};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  ctx.params.state = sc_state_power_off; /* wrong state */
  atr_len          = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);

  TEST_ASSERT_EQUAL(sc_Status_Bad_State, r);
}

/* ── ATR: T=0 with TA1+TC1 present ──────────────────────────────────────── */
void test_atr_ta1_tc1(void) {
  /* T0=0x50: TA1(0x10)+TC1(0x40) present, K=0
   * TA1=0x97 (Fi=9=512, Di=7=64), TC1=0x08 (N=8 extra guard time)
   */
  static const uint8_t raw[] = {0x3B, 0x50, 0x97, 0x08};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(4, atr_len);
  TEST_ASSERT_TRUE(ctx.params.ATR.T[0][ATR_INTERFACE_A].present);
  TEST_ASSERT_EQUAL_HEX8(0x97, ctx.params.ATR.T[0][ATR_INTERFACE_A].value);
  TEST_ASSERT_TRUE(ctx.params.ATR.T[0][ATR_INTERFACE_C].present);
  TEST_ASSERT_EQUAL_HEX8(0x08, ctx.params.ATR.T[0][ATR_INTERFACE_C].value);
}

/* ── ATR: bad TCK (wrong XOR checksum) ──────────────────────────────────── */
void test_atr_bad_tck(void) {
  /* T=1 ATR: 3B 80 01 → TCK must be 0x80^0x01=0x81; inject 0xFF instead */
  static const uint8_t raw[] = {0x3B, 0x80, 0x01, 0xFF};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);

  TEST_ASSERT_EQUAL(sc_Status_ATR_Malformed, r);
}

/* ── ATR: timeout on first byte ──────────────────────────────────────────── */
void test_atr_timeout(void) {
  slot_sim_setup(NULL, 0, NULL, 0); /* no bytes → timeout */
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);

  TEST_ASSERT_EQUAL(sc_Status_Slot_Reception_Timeout, r);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_atr_minimal_t0);
  RUN_TEST(test_atr_with_ta1);
  RUN_TEST(test_atr_historical_bytes);
  RUN_TEST(test_atr_t1_with_tck);
  RUN_TEST(test_atr_reverse_convention);
  RUN_TEST(test_atr_bad_ts);
  RUN_TEST(test_atr_buffer_too_small);
  RUN_TEST(test_atr_bad_state);
  RUN_TEST(test_atr_ta1_tc1);
  RUN_TEST(test_atr_bad_tck);
  RUN_TEST(test_atr_timeout);
  return UNITY_END();
}
