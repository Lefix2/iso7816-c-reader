#include "unity.h"

#include "EDC.h"
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

/* ── Complete Case-3 TPDU (send only): exercises normal send path ─────────── */
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

/* ── N=0xFF → GT=12 (no guardtime formula) ──────────────────────────────── */
void test_tpdu_t0_n_ff(void) {
  uint8_t  hdr[5]  = {0x00, 0xB0, 0x00, 0x00, 0x01};
  uint8_t  recv[8];
  uint32_t recv_len = 3; /* 1 data + 2 SW */

  static const uint8_t card_resp[] = {0xB0, 0xAB, 0x90, 0x00};
  uint8_t              tx_cap[32];

  setup_t0_context();
  ctx.params.N = 0xFF; /* triggers GT = 12 path */
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, hdr, sizeof(hdr), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
}

/* ── T=15 protocol present → extended GT formula ─────────────────────────── */
void test_tpdu_t0_t15_gt_formula(void) {
  uint8_t  hdr[5]  = {0x00, 0xB0, 0x00, 0x00, 0x01};
  uint8_t  recv[8];
  uint32_t recv_len = 3;

  static const uint8_t card_resp[] = {0xB0, 0xAB, 0x90, 0x00};
  uint8_t              tx_cap[32];

  setup_t0_context();
  ctx.params.supported_prot |= (uint16_t)(0x0001u << SC_PROTOCOL_T15);
  ctx.params.N = 1; /* nonzero so formula is non-trivial */
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, hdr, sizeof(hdr), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
}

/* ── D=64 → GT clamped to minimum 16 etu ────────────────────────────────── */
void test_tpdu_t0_d64_gt_min(void) {
  uint8_t  hdr[5]  = {0x00, 0xB0, 0x00, 0x00, 0x01};
  uint8_t  recv[8];
  uint32_t recv_len = 3;

  static const uint8_t card_resp[] = {0xB0, 0xAB, 0x90, 0x00};
  uint8_t              tx_cap[32];

  setup_t0_context();
  ctx.params.D = 64; /* triggers GT = max(GT, 16) clamping */
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, hdr, sizeof(hdr), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
}

/* ── set_timeout_etu failure ─────────────────────────────────────────────── */
void test_tpdu_t0_set_timeout_fail(void) {
  uint8_t  hdr[5] = {0x00, 0xB0, 0x00, 0x00, 0x00};
  uint8_t  recv[8];
  uint32_t recv_len = 2;

  setup_t0_context();
  slot_sim_setup(NULL, 0, NULL, 0);
  slot_sim_get_ctx()->set_timeout_fail_countdown = 1; /* fail first call */

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, hdr, sizeof(hdr), recv, &recv_len);

  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── set_guardtime_etu failure ───────────────────────────────────────────── */
void test_tpdu_t0_set_guardtime_fail(void) {
  uint8_t  hdr[5] = {0x00, 0xB0, 0x00, 0x00, 0x00};
  uint8_t  recv[8];
  uint32_t recv_len = 2;

  setup_t0_context();
  slot_sim_setup(NULL, 0, NULL, 0);
  slot_sim_get_ctx()->set_guardtime_fail_countdown = 1; /* fail first call */

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, hdr, sizeof(hdr), recv, &recv_len);

  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── recv_len-2 != P3 when rcv TPDU → Bad_Length ────────────────────────── */
void test_tpdu_t0_recv_p3_mismatch(void) {
  /* P3=2 but recv_len=5 means 3 bytes expected — mismatch */
  uint8_t  hdr[5]  = {0x00, 0xB0, 0x00, 0x00, 0x02};
  uint8_t  recv[8];
  uint32_t recv_len = 5; /* 5-2 = 3 != P3=2 */

  setup_t0_context();
  slot_sim_setup(NULL, 0, NULL, 0);

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, hdr, sizeof(hdr), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_TPDU_T0_Bad_Length, r);
}

/* ── send_len-5 != P3 when snd TPDU → Bad_Length ────────────────────────── */
void test_tpdu_t0_send_p3_mismatch(void) {
  /* P3=3 but only 1 data byte provided: 7-5=2 != 3 */
  uint8_t  tpdu[7]  = {0x00, 0xD6, 0x00, 0x00, 0x03, 0xAA, 0xBB};
  uint8_t  recv[8];
  uint32_t recv_len = 2;

  setup_t0_context();
  slot_sim_setup(NULL, 0, NULL, 0);

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, tpdu, sizeof(tpdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_TPDU_T0_Bad_Length, r);
}

/* ── send_bytes failure (header) ─────────────────────────────────────────── */
void test_tpdu_t0_send_bytes_fail(void) {
  uint8_t  tpdu[7] = {0x00, 0xD6, 0x00, 0x00, 0x02, 0xAA, 0xBB};
  uint8_t  recv[8];
  uint32_t recv_len = 2;

  setup_t0_context();
  slot_sim_setup(NULL, 0, NULL, 0);
  slot_sim_get_ctx()->send_fail_countdown = 1; /* fail first send_bytes */

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, tpdu, sizeof(tpdu), recv, &recv_len);

  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── Invalid procedure byte → Bad_Proc_byte ──────────────────────────────── */
void test_tpdu_t0_bad_proc_byte(void) {
  /* INS=0xD6, proc byte 0x12 is not NULL, not SW, not ACK */
  uint8_t  tpdu[7] = {0x00, 0xD6, 0x00, 0x00, 0x02, 0xAA, 0xBB};
  uint8_t  recv[8];
  uint32_t recv_len = 2;

  static const uint8_t card_resp[] = {0x12}; /* invalid proc byte */
  uint8_t              tx_cap[32];

  setup_t0_context();
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, tpdu, sizeof(tpdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_TPDU_T0_Bad_Proc_byte, r);
}

/* ── NULL proc byte (0x60) wait-loop, then normal ACK ───────────────────── */
void test_tpdu_t0_null_byte_then_ack(void) {
  uint8_t  tpdu[7] = {0x00, 0xD6, 0x00, 0x00, 0x02, 0xAA, 0xBB};
  uint8_t  recv[8];
  uint32_t recv_len = 2;

  /* 0x60 = NULL byte, then ACK (0xD6), then SW */
  static const uint8_t card_resp[] = {0x60, 0xD6, 0x90, 0x00};
  uint8_t              tx_cap[32];

  setup_t0_context();
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, tpdu, sizeof(tpdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
}

/* ── Single-byte ACK (INS^0xFF) receive path ─────────────────────────────── */
void test_tpdu_t0_single_byte_ack_recv(void) {
  /* INS=0xB0, single-byte-ACK = 0xB0^0xFF = 0x4F */
  uint8_t  hdr[5]  = {0x00, 0xB0, 0x00, 0x00, 0x01};
  uint8_t  recv[8];
  uint32_t recv_len = 3; /* 1 data + 2 SW */

  /* inscmplt=0x4F → receive 1 byte → SW */
  static const uint8_t card_resp[] = {0x4F, 0xCA, 0x90, 0x00};
  uint8_t              tx_cap[32];

  setup_t0_context();
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, hdr, sizeof(hdr), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(3, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0xCA, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0x90, recv[1]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[2]);
}

/* ── Single-byte ACK (INS^0xFF) send path ───────────────────────────────── */
void test_tpdu_t0_single_byte_ack_send(void) {
  /* INS=0xD6, single-byte-ACK = 0xD6^0xFF = 0x29 */
  uint8_t  tpdu[6] = {0x00, 0xD6, 0x00, 0x00, 0x01, 0xAA};
  uint8_t  recv[8];
  uint32_t recv_len = 2;

  /* inscmplt=0x29 → send 1 byte → SW */
  static const uint8_t card_resp[] = {0x29, 0x90, 0x00};
  uint8_t              tx_cap[32];

  setup_t0_context();
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, tpdu, sizeof(tpdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len);
}

/* ── SW2 receive failure: SW1 received but SW2 times out ────────────────── */
void test_tpdu_t0_sw2_fail(void) {
  /* P3=0, recv_len=2: only SW expected; sim provides SW1=0x90 then times out */
  uint8_t  hdr[5]  = {0x00, 0xB0, 0x00, 0x00, 0x00};
  uint8_t  recv[8];
  uint32_t recv_len = 2;

  static const uint8_t card_resp[] = {0x90}; /* SW1 only, SW2 missing */
  uint8_t              tx_cap[32];

  setup_t0_context();
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, hdr, sizeof(hdr), recv, &recv_len);

  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── receive_bytes failure mid-data (ACK then truncated data) ────────────── */
void test_tpdu_t0_recv_bytes_fail(void) {
  /* P3=3, recv_len=5: expect 3 data bytes; sim only provides ACK + 1 byte */
  uint8_t  hdr[5]  = {0x00, 0xB0, 0x00, 0x00, 0x03};
  uint8_t  recv[8];
  uint32_t recv_len = 5;

  static const uint8_t card_resp[] = {0xB0, 0xCA}; /* ACK + 1/3 bytes */
  uint8_t              tx_cap[32];

  setup_t0_context();
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, hdr, sizeof(hdr), recv, &recv_len);

  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── Receive buffer overflow on double ACK ───────────────────────────────── */
void test_tpdu_t0_recv_overflow(void) {
  /* P3=2, recv_len=4: after receiving 2 bytes, another ACK arrives */
  uint8_t  hdr[5]  = {0x00, 0xB0, 0x00, 0x00, 0x02};
  uint8_t  recv[8];
  uint32_t recv_len = 4;

  /* ACK + 2 data bytes + another ACK (should have been SW) */
  static const uint8_t card_resp[] = {0xB0, 0xCA, 0xFE, 0xB0};
  uint8_t              tx_cap[32];

  setup_t0_context();
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, hdr, sizeof(hdr), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_TPDU_T0_Bad_Length, r);
}

/* ── Send buffer overflow on double ACK ──────────────────────────────────── */
void test_tpdu_t0_send_overflow(void) {
  /* P3=1: after sending 1 data byte, another ACK arrives */
  uint8_t  tpdu[6] = {0x00, 0xD6, 0x00, 0x00, 0x01, 0xAA};
  uint8_t  recv[8];
  uint32_t recv_len = 2;

  /* ACK + ACK (second should have been SW) */
  static const uint8_t card_resp[] = {0xD6, 0xD6};
  uint8_t              tx_cap[32];

  setup_t0_context();
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, tpdu, sizeof(tpdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_TPDU_T0_Bad_Length, r);
}

/* ── send_bytes failure in transact_remaining_bytes ─────────────────────── */
void test_tpdu_t0_send_remaining_fail(void) {
  /* send P3=1 TPDU; first send_bytes (header) OK, second fails */
  uint8_t  tpdu[6] = {0x00, 0xD6, 0x00, 0x00, 0x01, 0xAA};
  uint8_t  recv[8];
  uint32_t recv_len = 2;

  static const uint8_t card_resp[] = {0xD6}; /* ACK triggers send_bytes again */
  uint8_t              tx_cap[32];

  setup_t0_context();
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  slot_sim_get_ctx()->send_fail_countdown = 2; /* fail 2nd send_bytes */

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, tpdu, sizeof(tpdu), recv, &recv_len);

  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── Single-byte receive failure (inscmplt then timeout) ─────────────────── */
void test_tpdu_t0_single_byte_recv_fail(void) {
  /* inscmplt=0x4F triggers transact_remaining_byte receive; no data → timeout */
  uint8_t  hdr[5]  = {0x00, 0xB0, 0x00, 0x00, 0x01};
  uint8_t  recv[8];
  uint32_t recv_len = 3;

  static const uint8_t card_resp[] = {0x4F}; /* inscmplt only, data missing */
  uint8_t              tx_cap[32];

  setup_t0_context();
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, hdr, sizeof(hdr), recv, &recv_len);

  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── Single-byte send failure (inscmplt then send_byte fails) ────────────── */
void test_tpdu_t0_single_byte_send_fail(void) {
  /* inscmplt=0x29 triggers transact_remaining_byte send; send_byte fails */
  uint8_t  tpdu[6] = {0x00, 0xD6, 0x00, 0x00, 0x01, 0xAA};
  uint8_t  recv[8];
  uint32_t recv_len = 2;

  static const uint8_t card_resp[] = {0x29}; /* inscmplt, triggers send_byte */
  uint8_t              tx_cap[32];

  setup_t0_context();
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  slot_sim_get_ctx()->send_fail_countdown = 2; /* header OK, send_byte fails */

  sc_Status r = protocol_TPDU_T0.Transact(&ctx, tpdu, sizeof(tpdu), recv, &recv_len);

  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_tpdu_t0_invalid_params);
  RUN_TEST(test_tpdu_t0_bad_state);
  RUN_TEST(test_tpdu_t0_both_send_and_receive);
  RUN_TEST(test_tpdu_t0_bad_header);
  RUN_TEST(test_tpdu_t0_send_path);
  RUN_TEST(test_tpdu_t0_receive_path);
  RUN_TEST(test_tpdu_t0_n_ff);
  RUN_TEST(test_tpdu_t0_t15_gt_formula);
  RUN_TEST(test_tpdu_t0_d64_gt_min);
  RUN_TEST(test_tpdu_t0_set_timeout_fail);
  RUN_TEST(test_tpdu_t0_set_guardtime_fail);
  RUN_TEST(test_tpdu_t0_recv_p3_mismatch);
  RUN_TEST(test_tpdu_t0_send_p3_mismatch);
  RUN_TEST(test_tpdu_t0_send_bytes_fail);
  RUN_TEST(test_tpdu_t0_bad_proc_byte);
  RUN_TEST(test_tpdu_t0_null_byte_then_ack);
  RUN_TEST(test_tpdu_t0_single_byte_ack_recv);
  RUN_TEST(test_tpdu_t0_single_byte_ack_send);
  RUN_TEST(test_tpdu_t0_sw2_fail);
  RUN_TEST(test_tpdu_t0_recv_bytes_fail);
  RUN_TEST(test_tpdu_t0_recv_overflow);
  RUN_TEST(test_tpdu_t0_send_overflow);
  RUN_TEST(test_tpdu_t0_send_remaining_fail);
  RUN_TEST(test_tpdu_t0_single_byte_recv_fail);
  RUN_TEST(test_tpdu_t0_single_byte_send_fail);
  return UNITY_END();
}
