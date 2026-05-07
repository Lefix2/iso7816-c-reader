#include "unity.h"

#include "protocols.h"
#include "sc_context.h"
#include "sc_defs.h"
#include "slot_sim.h"

static sc_context_t ctx;

static void setup_t0_context(void) {
  iso_params_init(&ctx.params);
  ctx.params.state     = sc_state_active_on_t0;
  ctx.params.F         = SC_Fd;
  ctx.params.D         = SC_Dd;
  ctx.params.Fi        = SC_Fd;
  ctx.params.Di        = SC_Dd;
  ctx.params.fmax      = SC_fmaxd;
  ctx.params.frequency = SC_fmaxd;
  ctx.params.WI        = ATR_DEFAULT_WI;
  ctx.params.N         = ATR_DEFAULT_N;
  ctx.slot             = &hslot_sim;
}

void setUp(void) {}
void tearDown(void) {}

/* ── Tiny receive buffer (< 2) → Invalid_Parameter ──────────────────────── */
void test_tpdu_t0_invalid_params(void) {
  uint8_t  hdr[5] = {0x00, 0x20, 0x00, 0x00, 0x00};
  uint8_t  buf[8];
  uint32_t len;

  setup_t0_context();
  slot_sim_setup(NULL, 0, NULL, 0);
  len         = 1; /* < 2 */
  sc_Status r = protocol_TPDU_T0.Transact(&ctx, hdr, 5, buf, &len);
  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, r);
}

/* ── Wrong protocol state → Bad_State ───────────────────────────────────── */
void test_tpdu_t0_bad_state(void) {
  uint8_t  hdr[5] = {0x00, 0x20, 0x00, 0x00, 0x00};
  uint8_t  buf[8];
  uint32_t len = sizeof(buf);

  setup_t0_context();
  ctx.params.state = sc_state_active_on_t1; /* wrong */
  slot_sim_setup(NULL, 0, NULL, 0);

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, hdr, 5, buf, &len);
  TEST_ASSERT_EQUAL(sc_Status_Bad_State, r);
}

/* ── Both send>5 and receive>2 → TPDU_T0_Bad_Length ─────────────────────── */
void test_tpdu_t0_both_send_and_receive(void) {
  uint8_t  hdr[6] = {0x00, 0x20, 0x00, 0x00, 0x01, 0xAA};
  uint8_t  buf[8];
  uint32_t len = 5; /* > 2 */

  setup_t0_context();
  slot_sim_setup(NULL, 0, NULL, 0);

  /* len_to_send=6 > 5 AND len_to_receive=5 > 2 */
  sc_Status r = protocol_TPDU_T0.Transact(&ctx, hdr, 6, buf, &len);
  TEST_ASSERT_EQUAL(sc_Status_TPDU_T0_Bad_Length, r);
}

/* ── len_to_send < 5 → TPDU_T0_Bad_Header ───────────────────────────────── */
void test_tpdu_t0_bad_header(void) {
  uint8_t  hdr[4] = {0x00, 0x20, 0x00, 0x00}; /* only 4 bytes */
  uint8_t  buf[8];
  uint32_t len = sizeof(buf);

  setup_t0_context();
  slot_sim_setup(NULL, 0, NULL, 0);

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, hdr, 4, buf, &len);
  TEST_ASSERT_EQUAL(sc_Status_TPDU_T0_Bad_Header, r);
}

/* ── Complete Case-3 TPDU (send only): exercises normal send path ───────────
 */
void test_tpdu_t0_send_path(void) {
  /* TPDU: header [00 D6 00 00 02] + 2 data bytes, rcv=2 (SW only) */
  uint8_t  tpdu[7] = {0x00, 0xD6, 0x00, 0x00, 0x02, 0xAA, 0xBB};
  uint8_t  recv[8];
  uint32_t recv_len = 2; /* just SW */

  /* Card: ACK=INS then SW */
  static const uint8_t card_resp[] = {0xD6, 0x90, 0x00};
  uint8_t              tx_cap[32];

  setup_t0_context();
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));

  sc_Status r =
      protocol_TPDU_T0.Transact(&ctx, tpdu, sizeof(tpdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0x90, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[1]);
}

/* ── Complete Case-2 TPDU (receive only): exercises receive path ─────────── */
void test_tpdu_t0_receive_path(void) {
  /* TPDU: header [00 B0 00 00 02], rcv=2+2=4 (2 data + 2 SW) */
  uint8_t  hdr[5] = {0x00, 0xB0, 0x00, 0x00, 0x02};
  uint8_t  recv[8];
  uint32_t recv_len = 4;

  /* Card: ACK=INS, 2 data bytes, SW */
  static const uint8_t card_resp[] = {0xB0, 0xCA, 0xFE, 0x90, 0x00};
  uint8_t              tx_cap[32];

  setup_t0_context();
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));

  sc_Status r =
      protocol_TPDU_T0.Transact(&ctx, hdr, sizeof(hdr), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(4, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0xCA, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0xFE, recv[1]);
  TEST_ASSERT_EQUAL_HEX8(0x90, recv[2]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[3]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_tpdu_t0_invalid_params);
  RUN_TEST(test_tpdu_t0_bad_state);
  RUN_TEST(test_tpdu_t0_both_send_and_receive);
  RUN_TEST(test_tpdu_t0_bad_header);
  RUN_TEST(test_tpdu_t0_send_path);
  RUN_TEST(test_tpdu_t0_receive_path);
  return UNITY_END();
}
