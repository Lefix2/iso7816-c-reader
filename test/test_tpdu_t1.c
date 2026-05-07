#include <string.h>

#include "unity.h"

#include "maths/EDC.h"
#include "protocols/protocols.h"
#include "sc_context.h"
#include "sc_defs.h"
#include "slot_sim.h"

static sc_context_t ctx;

static void setup_t1_context(void) {
  iso_params_init(&ctx.params);
  ctx.params.state     = sc_state_active_on_t1;
  ctx.params.F         = SC_Fd;
  ctx.params.D         = SC_Dd;
  ctx.params.Fi        = SC_Fd;
  ctx.params.Di        = SC_Dd;
  ctx.params.fmax      = SC_fmaxd;
  ctx.params.frequency = SC_fmaxd;
  ctx.params.N         = ATR_DEFAULT_N;
  ctx.params.EDC       = SC_EDC_LRC;
  ctx.params.IFSC      = ATR_DEFAULT_IFS;
  ctx.params.IFSD      = ATR_DEFAULT_IFS;
  ctx.params.CWI       = ATR_DEFAULT_CWI;
  ctx.params.BWI       = ATR_DEFAULT_BWI;
  ctx.params.Nd        = 0;
  ctx.params.Nc        = 0;
  ctx.params.DAD       = ATR_DEFAULT_DAD;
  ctx.params.SAD       = ATR_DEFAULT_SAD;
  ctx.slot             = &hslot_sim;
}

/* Build a valid LRC I-block to send to TPDU */
static void build_send_i_block_lrc(uint8_t  *block,
                                   uint32_t *len,
                                   uint8_t   ns,
                                   uint8_t   data_byte) {
  block[0]     = 0x00;
  block[1]     = (uint8_t)(ns << 6);
  block[2]     = 0x01;
  block[3]     = data_byte;
  block[4]     = EDC_LRC(block, 4);
  *len         = 5;
}

static void build_send_i_block_crc(uint8_t  *block,
                                   uint32_t *len,
                                   uint8_t   ns,
                                   uint8_t   data_byte) {
  block[0]      = 0x00;
  block[1]      = (uint8_t)(ns << 6);
  block[2]      = 0x01;
  block[3]      = data_byte;
  uint16_t crc  = EDC_CRC(block, 4);
  block[4]      = (uint8_t)(crc >> 8);
  block[5]      = (uint8_t)(crc & 0xFF);
  *len          = 6;
}

void setUp(void) {}
void tearDown(void) {}

/* ── Tiny receive buffer (< 4) → Invalid_Parameter ──────────────────────── */
void test_tpdu_t1_invalid_params(void) {
  uint8_t  block[8] = {0};
  uint32_t len;

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);
  len       = 3; /* < 4 */
  sc_Status r = protocol_TPDU_T1.Transact(&ctx, block, 5, block, &len);
  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, r);
}

/* ── Wrong state → Bad_State ────────────────────────────────────────────── */
void test_tpdu_t1_bad_state(void) {
  uint8_t  block[8];
  uint32_t len = sizeof(block);

  setup_t1_context();
  ctx.params.state = sc_state_active_on_t0;
  slot_sim_setup(NULL, 0, NULL, 0);

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, block, 5, block, &len);
  TEST_ASSERT_EQUAL(sc_Status_Bad_State, r);
}

/* ── len_to_send mismatch with LEN field → Bad_Length ───────────────────── */
void test_tpdu_t1_bad_length(void) {
  /* LEN byte says 1, but we pass 7 bytes (not 3+1+1=5) */
  uint8_t send[7] = {0x00, 0x00, 0x01, 0xAA, 0xAB, 0x00, 0x00};
  uint8_t recv[32];
  uint32_t recv_len = sizeof(recv);

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, 7, recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_TPDU_T1_Bad_Length, r);
}

/* ── LRC mismatch in card response → Bad_EDC ────────────────────────────── */
void test_tpdu_t1_bad_edc_lrc(void) {
  /* Send a valid I-block, card responds with wrong LRC */
  uint8_t  send[5];
  uint32_t slen;
  build_send_i_block_lrc(send, &slen, 0, 0xAA);

  /* Card response: valid prologue+data but LRC=0x00 (wrong) */
  uint8_t sim_rx[5] = {0x00, 0x00, 0x01, 0xFF, 0x00}; /* LRC should be 0xFE */

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);
  uint8_t  tx_cap[32];

  setup_t1_context();
  slot_sim_setup(sim_rx, sizeof(sim_rx), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_TPDU_T1_Bad_EDC, r);
}

/* ── CRC mode: valid CRC exchange succeeds ──────────────────────────────── */
void test_tpdu_t1_crc_mode(void) {
  /* Send I-block with CRC; card responds with I-block with correct CRC */
  uint8_t  send[6];
  uint32_t slen;
  build_send_i_block_crc(send, &slen, 0, 0xAA);

  /* Build card response with correct CRC */
  uint8_t  resp_data[4] = {0x00, 0x00, 0x01, 0xFF};
  uint16_t crc          = EDC_CRC(resp_data, 4);
  uint8_t  sim_rx[6]    = {resp_data[0], resp_data[1], resp_data[2], resp_data[3],
                            (uint8_t)(crc >> 8), (uint8_t)(crc & 0xFF)};

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);
  uint8_t  tx_cap[32];

  setup_t1_context();
  ctx.params.EDC = SC_EDC_CRC;
  slot_sim_setup(sim_rx, sizeof(sim_rx), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);
}

/* ── CRC mode: wrong CRC in card response → Bad_EDC ────────────────────── */
void test_tpdu_t1_bad_edc_crc(void) {
  uint8_t  send[6];
  uint32_t slen;
  build_send_i_block_crc(send, &slen, 0, 0xAA);

  /* Card response with deliberately wrong CRC */
  uint8_t sim_rx[6] = {0x00, 0x00, 0x01, 0xFF, 0x00, 0x00}; /* CRC=0x0000, wrong */

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);
  uint8_t  tx_cap[32];

  setup_t1_context();
  ctx.params.EDC = SC_EDC_CRC;
  slot_sim_setup(sim_rx, sizeof(sim_rx), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_TPDU_T1_Bad_EDC, r);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_tpdu_t1_invalid_params);
  RUN_TEST(test_tpdu_t1_bad_state);
  RUN_TEST(test_tpdu_t1_bad_length);
  RUN_TEST(test_tpdu_t1_bad_edc_lrc);
  RUN_TEST(test_tpdu_t1_crc_mode);
  RUN_TEST(test_tpdu_t1_bad_edc_crc);
  return UNITY_END();
}
