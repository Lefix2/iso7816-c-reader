#include "unity.h"

#include "protocols.h"
#include "sc_context.h"
#include "sc_defs.h"
#include "slot_sim.h"
#include "smartcard.h"

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

/* ── Case 1: no data in, no data out ─────────────────────────────────────── */
void test_apdu_t0_case1(void) {
  /* APDU: CLA=00 INS=20 P1=00 P2=00 (4 bytes, Case 1) */
  uint8_t apdu[] = {0x00, 0x20, 0x00, 0x00};
  /* Card: SW1=0x90, SW2=0x00 */
  static const uint8_t card_resp[] = {0x90, 0x00};
  uint8_t              tx_cap[32];
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0x90, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[1]);
}

/* ── Case 2S: receive Le bytes (Le=2) ────────────────────────────────────── */
void test_apdu_t0_case2s(void) {
  /* APDU: READ BINARY, read 2 bytes */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x02};
  /* Card: INS ACK, then 2 data bytes, then SW */
  static const uint8_t card_resp[] = {0xB0, 0xDE, 0xAD, 0x90, 0x00};
  uint8_t              tx_cap[32];
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(4, recv_len); /* 2 data + 2 SW */
  TEST_ASSERT_EQUAL_HEX8(0xDE, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0xAD, recv[1]);
  TEST_ASSERT_EQUAL_HEX8(0x90, recv[2]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[3]);
}

/* ── Case 3S: send Nc=3 bytes ────────────────────────────────────────────── */
void test_apdu_t0_case3s(void) {
  /* APDU: UPDATE BINARY, write 3 bytes */
  uint8_t apdu[] = {0x00, 0xD6, 0x00, 0x00, 0x03, 0xAA, 0xBB, 0xCC};
  /* Card: INS ACK → send data, then SW */
  static const uint8_t card_resp[] = {0xD6, 0x90, 0x00};
  uint8_t              tx_cap[32];
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len); /* SW only */
  /* Verify we sent header + data */
  TEST_ASSERT_EQUAL(8, slot_sim_get_ctx()->tx_pos);
  TEST_ASSERT_EQUAL_HEX8(0xAA, tx_cap[5]);
  TEST_ASSERT_EQUAL_HEX8(0xBB, tx_cap[6]);
  TEST_ASSERT_EQUAL_HEX8(0xCC, tx_cap[7]);
}

/* ── Case 4S: send Nc=2 bytes, receive Ne=1 byte ─────────────────────────── */
void test_apdu_t0_case4s(void) {
  /* APDU: CLA INS P1 P2 Lc=2 data[2] Le=1 */
  uint8_t apdu[] = {0x00, 0x88, 0x00, 0x00, 0x02, 0x01, 0x02, 0x01};
  /* Card: INS ACK for send, then SW */
  static const uint8_t card_resp[] = {0x88, 0x90, 0x00};
  uint8_t              tx_cap[32];
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
}

/* ── 0x6C: wrong length, card specifies Na ───────────────────────────────── */
void test_apdu_t0_wrong_length_6c(void) {
  /* READ BINARY asking 3 bytes, card says only 1 available (SW=6C 01) */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x03};
  /* First response: 0x6C 0x01 (send again with Le=1)
   * Then: INS ACK, 1 data byte, 0x90 0x00 */
  static const uint8_t card_resp[] = {0x6C, 0x01, 0xB0, 0xFF, 0x90, 0x00};
  uint8_t              tx_cap[32];
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(3, recv_len); /* 1 data + SW */
  TEST_ASSERT_EQUAL_HEX8(0xFF, recv[0]);
}

/* ── 0x61: GET RESPONSE chaining ─────────────────────────────────────────── */
void test_apdu_t0_get_response_61(void) {
  /* Case 2S: Le=2. Card says 0x61 0x02 → need GET RESPONSE */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x02};
  /* First TPDU: send header, receive 0x61 0x02
   * Second TPDU (GET RESPONSE): send [CLA C0 00 00 02], receive ACK 0xC0, data
   * 0xDE 0xAD, 0x90 0x00 */
  static const uint8_t card_resp[] = {
      0x61, 0x02, /* SW1=0x61: use GET RESPONSE, SW2=len */
      0xC0,       /* ACK for GET RESPONSE */
      0xDE, 0xAD, /* response data */
      0x90, 0x00  /* final SW */
  };
  uint8_t tx_cap[64];
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(4, recv_len); /* 2 data + SW */
  TEST_ASSERT_EQUAL_HEX8(0xDE, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0xAD, recv[1]);
  TEST_ASSERT_EQUAL_HEX8(0x90, recv[2]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[3]);
}

/* ── Timeout: card never responds ───────────────────────────────────────── */
void test_apdu_t0_timeout(void) {
  uint8_t apdu[] = {0x00, 0x20, 0x00, 0x00};
  slot_sim_setup(NULL, 0, NULL, 0); /* no rx bytes → timeout */
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Slot_Reception_Timeout, r);
}

/* ── Malformed APDU ──────────────────────────────────────────────────────── */
void test_apdu_t0_malformed(void) {
  /* 3 bytes is not a valid APDU */
  uint8_t apdu[] = {0x00, 0x20, 0x00};
  slot_sim_setup(NULL, 0, NULL, 0);
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, r);
}

/* ── NULL context → Invalid_Parameter ───────────────────────────────────── */
void test_apdu_t0_null_context(void) {
  uint8_t  apdu[] = {0x00, 0x20, 0x00, 0x00};
  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(NULL, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, r);
}

/* ── Wrong protocol state → Bad_State ───────────────────────────────────── */
void test_apdu_t0_bad_state(void) {
  uint8_t apdu[] = {0x00, 0x20, 0x00, 0x00};
  slot_sim_setup(NULL, 0, NULL, 0);
  setup_t0_context();
  ctx.params.state = sc_state_active_on_t1;

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_Bad_State, r);
}

/* ── Buffer smaller than Ne+2 → Buffer_To_Small ─────────────────────────── */
void test_apdu_t0_buffer_too_small_for_ne(void) {
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x0A}; /* Le=10, Ne=10 */
  slot_sim_setup(NULL, 0, NULL, 0);
  setup_t0_context();

  uint8_t  recv[8]; /* 8 < 10+2=12 */
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_Buffer_To_Small, r);
}

/* ── Case 2E: extended receive, C5=0, n=7 ────────────────────────────────── */
void test_apdu_t0_case2e(void) {
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x03}; /* Ne=3 */
  static const uint8_t card_resp[] = {0xB0, 0xDE, 0xAD, 0xBE, 0x90, 0x00};
  uint8_t              tx_cap[32];
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(5, recv_len); /* 3 data + 2 SW */
  TEST_ASSERT_EQUAL_HEX8(0xDE, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0xAD, recv[1]);
  TEST_ASSERT_EQUAL_HEX8(0xBE, recv[2]);
  TEST_ASSERT_EQUAL_HEX8(0x90, recv[3]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[4]);
}

/* ── Case 3E small Nc: extended send, Nc=2<256 (takes Case_4S path) ──────── */
void test_apdu_t0_case3e_small(void) {
  /* C5=0, C6C7=2, n==7+2 → Case_3E, then Nc<256 → Case_4S → Send */
  uint8_t apdu[] = {0x00, 0xD6, 0x00, 0x00, 0x00, 0x00, 0x02, 0xAA, 0xBB};
  static const uint8_t card_resp[] = {0xD6, 0x90, 0x00};
  uint8_t              tx_cap[32];
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len); /* SW only */
}

/* ── Case 4E small Nc: extended send+receive, Nc=2<256 ──────────────────── */
void test_apdu_t0_case4e_small(void) {
  /* C5=0, C6C7=2, n==9+2 → Case_4E, Ne=1, then Nc<256 → Case_4S → Send */
  uint8_t              apdu[]      = {0x00, 0x88, 0x00, 0x00, 0x00, 0x00,
                                      0x02, 0xAA, 0xBB, 0x00, 0x01};
  static const uint8_t card_resp[] = {0x88, 0x90, 0x00};
  uint8_t              tx_cap[32];
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len); /* 9000 patch: ends immediately */
}

/* ── Malformed extended APDU → sc_Status_APDU_T0_Malformed ──────────────── */
void test_apdu_t0_malformed_extended(void) {
  /* n=6, C5=0, C6C7=5 — doesn't fit 2E(7), 3E(12), or 4E(14) */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x00, 0x05};
  slot_sim_setup(NULL, 0, NULL, 0);
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_APDU_T0_Malformed, r);
}

/* ── SW1=0x91: non-9000 0x9X terminates transaction as-is ───────────────── */
void test_apdu_t0_sw_9xyz(void) {
  uint8_t              apdu[]      = {0x00, 0x20, 0x00, 0x00};
  static const uint8_t card_resp[] = {0x91, 0x00};
  uint8_t              tx_cap[32];
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0x91, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[1]);
}

/* ── SW1=0x62: non-0x61/non-0x6C warning terminates transaction ──────────── */
void test_apdu_t0_sw_6xyz(void) {
  uint8_t              apdu[]      = {0x00, 0x20, 0x00, 0x00};
  static const uint8_t card_resp[] = {0x62, 0x00};
  uint8_t              tx_cap[32];
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0x62, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[1]);
}

/* ── NULL byte 0x60 from card before SW: exercises TPDU_T0 NULL-byte loop ── */
void test_apdu_t0_null_byte_proc(void) {
  uint8_t              apdu[]      = {0x00, 0x20, 0x00, 0x00};
  static const uint8_t card_resp[] = {0x60, 0x90, 0x00}; /* NULL then SW */
  uint8_t              tx_cap[32];
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0x90, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[1]);
}

/* ── Case 3E Envelope: Nc=256 exercises ENVELOPE TPDU loop ──────────────── */
void test_apdu_t0_case3e_envelope(void) {
  uint8_t apdu[263];
  apdu[0] = 0x00;
  apdu[1] = 0xD6;
  apdu[2] = 0x00;
  apdu[3] = 0x00;
  apdu[4] = 0x00;
  apdu[5] = 0x01;
  apdu[6] = 0x00; /* extended Lc=256 */
  for (int i = 0; i < 256; i++)
    apdu[7 + i] = (uint8_t)i;

  /* Chunk 1 (255 bytes) + Chunk 2 (1 byte) — each: ACK(C2) then 9000 */
  static const uint8_t card_resp[] = {0xC2, 0x90, 0x00, 0xC2, 0x90, 0x00};
  uint8_t              tx_cap[600];
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0x90, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[1]);
}

/* ── Debug hook: SC_DBG_COMM fires when hook registered ─────────────────── */
static int s_t0_hook_calls;
static void
t0_hook_counter(const char *tag, const uint8_t *data, uint32_t len) {
  (void)tag;
  (void)data;
  (void)len;
  s_t0_hook_calls++;
}

void test_apdu_t0_with_debug_hook(void) {
  uint8_t              apdu[]      = {0x00, 0x20, 0x00, 0x00};
  static const uint8_t card_resp[] = {0x90, 0x00};
  uint8_t              tx_cap[32];
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  setup_t0_context();

  s_t0_hook_calls = 0;
  smartcard_Set_Debug_Hook(t0_hook_counter);

  uint8_t   recv[16];
  uint32_t  recv_len = sizeof(recv);
  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  smartcard_Set_Debug_Hook(NULL);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_GREATER_THAN(0, s_t0_hook_calls);
}

/* ── Malformed short APDU (C5≠0, length matches neither 3S nor 4S) ──────── */
void test_apdu_t0_malformed_short(void) {
  /* C5=3 → 3S needs n=8, 4S needs n=9; send n=10 → APDU_T0_Malformed */
  uint8_t apdu[] = {0x00, 0x20, 0x00, 0x00, 0x03, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
  slot_sim_setup(NULL, 0, NULL, 0);
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_APDU_T0_Malformed, r);
}

/* ── SW1=0x61 but all Ne bytes already received → success, no GetResponse ── */
void test_apdu_t0_61_all_received(void) {
  /* Case 2S: Ne=1. Card returns: ACK(B0) + 1 data byte + SW=0x61 0x00.
   * After TPDU: receive_length=1=Ne → Ne-receive_length=0 → END_TRANSACTION. */
  uint8_t              apdu[]      = {0x00, 0xB0, 0x00, 0x00, 0x01};
  static const uint8_t card_resp[] = {0xB0, 0xFF, 0x61, 0x00};
  uint8_t              tx_cap[16];
  slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
  setup_t0_context();

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(3, recv_len); /* 1 data + SW1 + SW2 */
  TEST_ASSERT_EQUAL_HEX8(0xFF, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0x61, recv[1]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[2]);
}


int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_apdu_t0_case1);
  RUN_TEST(test_apdu_t0_case2s);
  RUN_TEST(test_apdu_t0_case3s);
  RUN_TEST(test_apdu_t0_case4s);
  RUN_TEST(test_apdu_t0_wrong_length_6c);
  RUN_TEST(test_apdu_t0_get_response_61);
  RUN_TEST(test_apdu_t0_timeout);
  RUN_TEST(test_apdu_t0_malformed);
  RUN_TEST(test_apdu_t0_null_context);
  RUN_TEST(test_apdu_t0_bad_state);
  RUN_TEST(test_apdu_t0_buffer_too_small_for_ne);
  RUN_TEST(test_apdu_t0_case2e);
  RUN_TEST(test_apdu_t0_case3e_small);
  RUN_TEST(test_apdu_t0_case4e_small);
  RUN_TEST(test_apdu_t0_malformed_extended);
  RUN_TEST(test_apdu_t0_sw_9xyz);
  RUN_TEST(test_apdu_t0_sw_6xyz);
  RUN_TEST(test_apdu_t0_null_byte_proc);
  RUN_TEST(test_apdu_t0_case3e_envelope);
  RUN_TEST(test_apdu_t0_with_debug_hook);
  RUN_TEST(test_apdu_t0_malformed_short);
  RUN_TEST(test_apdu_t0_61_all_received);
  return UNITY_END();
}
