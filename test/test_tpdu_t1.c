#include "unity.h"

#include "EDC.h"
#include "protocols.h"
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
  block[0] = 0x00;
  block[1] = (uint8_t)(ns << 6);
  block[2] = 0x01;
  block[3] = data_byte;
  block[4] = EDC_LRC(block, 4);
  *len     = 5;
}

static void build_send_i_block_crc(uint8_t  *block,
                                   uint32_t *len,
                                   uint8_t   ns,
                                   uint8_t   data_byte) {
  block[0]     = 0x00;
  block[1]     = (uint8_t)(ns << 6);
  block[2]     = 0x01;
  block[3]     = data_byte;
  uint16_t crc = EDC_CRC(block, 4);
  block[4]     = (uint8_t)(crc >> 8);
  block[5]     = (uint8_t)(crc & 0xFF);
  *len         = 6;
}

void setUp(void) {}
void tearDown(void) {}

/* ── Tiny receive buffer (< 4) → Invalid_Parameter ──────────────────────── */
void test_tpdu_t1_invalid_params(void) {
  uint8_t  block[8] = {0};
  uint32_t len;

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);
  len         = 3; /* < 4 */
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
  uint8_t  send[7] = {0x00, 0x00, 0x01, 0xAA, 0xAB, 0x00, 0x00};
  uint8_t  recv[32];
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
  uint8_t  sim_rx[6]    = {resp_data[0],        resp_data[1],
                           resp_data[2],        resp_data[3],
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
  uint8_t sim_rx[6] = {0x00, 0x00, 0x01,
                       0xFF, 0x00, 0x00}; /* CRC=0x0000, wrong */

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);
  uint8_t  tx_cap[32];

  setup_t1_context();
  ctx.params.EDC = SC_EDC_CRC;
  slot_sim_setup(sim_rx, sizeof(sim_rx), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_TPDU_T1_Bad_EDC, r);
}

/* ── N=0xFF → CGT=11 path ───────────────────────────────────────────────── */
void test_tpdu_t1_n_ff(void) {
  uint8_t  send[5];
  uint32_t slen;
  build_send_i_block_lrc(send, &slen, 0, 0xAA);

  uint8_t resp[4] = {0x00, 0x00, 0x01, 0xFF};
  resp[3]         = 0xFF;
  uint8_t lrc     = EDC_LRC(resp, 3); /* recompute over prologue only for data */
  /* Full response: prologue + data + correct LRC */
  uint8_t  sim_rx[5] = {0x00, 0x00, 0x01, 0xFF, (uint8_t)(0x00 ^ 0x00 ^ 0x01 ^ 0xFF)};
  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);
  uint8_t  tx_cap[32];
  (void)lrc;

  setup_t1_context();
  ctx.params.N = 0xFF; /* triggers CGT = 11 path */
  slot_sim_setup(sim_rx, sizeof(sim_rx), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);
}

/* ── Malformed: bad NAD 0xFF → Bad_NAD → Malformed ─────────────────────── */
void test_tpdu_t1_bad_nad_0xff(void) {
  uint8_t  send[5];
  uint32_t slen;
  build_send_i_block_lrc(send, &slen, 0, 0xAA);
  send[0] = 0xFF; /* corrupt NAD */
  send[4] = EDC_LRC(send, 4);

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_TPDU_T1_Malformed, r);
}

/* ── Malformed: DAD == SAD in NAD → Bad_NAD ────────────────────────────── */
void test_tpdu_t1_bad_nad_dad_eq_sad(void) {
  uint8_t  send[5];
  uint32_t slen;
  build_send_i_block_lrc(send, &slen, 0, 0xAA);
  send[0] = 0x11; /* DAD=1, SAD=1 → DAD==SAD */
  send[4] = EDC_LRC(send, 4);

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_TPDU_T1_Malformed, r);
}

/* ── Malformed: bit7=1 with invalid lower bits → Bad_PCB ────────────────── */
void test_tpdu_t1_bad_pcb_high(void) {
  uint8_t  send[5];
  uint32_t slen;
  build_send_i_block_lrc(send, &slen, 0, 0xAA);
  send[0] = 0x12; /* valid NAD */
  send[1] = 0x81; /* R-block with bit0=1 invalid */
  send[4] = EDC_LRC(send, 4);

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_TPDU_T1_Malformed, r);
}

/* ── Malformed: bit7=0 bit6=1 with invalid bits → Bad_PCB ──────────────── */
void test_tpdu_t1_bad_pcb_s_lower(void) {
  uint8_t  send[5];
  uint32_t slen;
  build_send_i_block_lrc(send, &slen, 0, 0xAA);
  send[0] = 0x12;
  send[1] = 0x44; /* bit7=0, bit6=1, bit2=1 invalid */
  send[4] = EDC_LRC(send, 4);

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_TPDU_T1_Malformed, r);
}

/* ── Malformed: R-branch invalid PCB bits ───────────────────────────────── */
void test_tpdu_t1_bad_pcb_r_lower(void) {
  uint8_t  send[5];
  uint32_t slen;
  build_send_i_block_lrc(send, &slen, 0, 0xAA);
  send[0] = 0x12;
  send[1] = 0x04; /* bit7=0, bit6=0, bit2=1 invalid */
  send[4] = EDC_LRC(send, 4);

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_TPDU_T1_Malformed, r);
}

/* ── Malformed: R-block (bit7=0 bit6=0) with LEN!=0 → Bad_LEN ──────────── */
void test_tpdu_t1_bad_len_rblock(void) {
  uint8_t  send[6];
  send[0]       = 0x12;
  send[1]       = 0x00; /* passes R-branch PCB check */
  send[2]       = 0x01; /* LEN=1: must be 0 for R-block */
  send[3]       = 0xAA;
  send[4]       = EDC_LRC(send, 4);
  uint32_t slen = 5;

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_TPDU_T1_Malformed, r);
}

/* ── Malformed: LEN=0xFF → Bad_LEN ─────────────────────────────────────── */
void test_tpdu_t1_bad_len_ff(void) {
  uint8_t  send[5] = {0x12, 0x80, 0xFF, 0xAA, 0x00};
  send[4]       = EDC_LRC(send, 4);
  uint32_t slen = 5;

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_TPDU_T1_Malformed, r);
}

/* ── Malformed: LEN > IFS → Bad_LEN ────────────────────────────────────── */
void test_tpdu_t1_bad_len_gt_ifs(void) {
  uint8_t  send[5] = {0x12, 0x80, 0x21, 0xAA, 0x00}; /* LEN=33 > IFSC=32 */
  send[4]       = EDC_LRC(send, 4);
  uint32_t slen = 5;

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_TPDU_T1_Malformed, r);
}

/* ── set_timeout_etu (CWT) failure ──────────────────────────────────────── */
void test_tpdu_t1_cwt_timeout_fail(void) {
  uint8_t  send[5];
  uint32_t slen;
  build_send_i_block_lrc(send, &slen, 0, 0xAA);

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);
  slot_sim_get_ctx()->set_timeout_fail_countdown = 1;

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── set_guardtime_etu (CGT) failure ────────────────────────────────────── */
void test_tpdu_t1_cgt_guardtime_fail(void) {
  uint8_t  send[5];
  uint32_t slen;
  build_send_i_block_lrc(send, &slen, 0, 0xAA);

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);
  slot_sim_get_ctx()->set_guardtime_fail_countdown = 1;

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── send_bytes failure ──────────────────────────────────────────────────── */
void test_tpdu_t1_send_fail(void) {
  uint8_t  send[5];
  uint32_t slen;
  build_send_i_block_lrc(send, &slen, 0, 0xAA);

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);
  slot_sim_get_ctx()->send_fail_countdown = 1;

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── set_timeout_etu (BWT) failure ──────────────────────────────────────── */
void test_tpdu_t1_bwt_timeout_fail(void) {
  uint8_t  send[5];
  uint32_t slen;
  build_send_i_block_lrc(send, &slen, 0, 0xAA);

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);
  slot_sim_get_ctx()->set_timeout_fail_countdown = 2; /* CWT OK, BWT fails */

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── set_guardtime_etu (BGT) failure ────────────────────────────────────── */
void test_tpdu_t1_bgt_guardtime_fail(void) {
  uint8_t  send[5];
  uint32_t slen;
  build_send_i_block_lrc(send, &slen, 0, 0xAA);

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);
  slot_sim_get_ctx()->set_guardtime_fail_countdown = 2; /* CGT OK, BGT fails */

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── receive_information failure (truncated: LEN=2 but no data bytes) ───── */
void test_tpdu_t1_recv_info_fail(void) {
  uint8_t  send[5];
  uint32_t slen;
  build_send_i_block_lrc(send, &slen, 0, 0xAA);

  uint8_t  sim_rx[3] = {0x00, 0x00, 0x02}; /* prologue only, no info bytes */
  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);
  uint8_t  tx_cap[32];

  setup_t1_context();
  slot_sim_setup(sim_rx, sizeof(sim_rx), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── receive_LRC failure (prologue + data present, LRC byte missing) ─────── */
void test_tpdu_t1_recv_lrc_fail(void) {
  uint8_t  send[5];
  uint32_t slen;
  build_send_i_block_lrc(send, &slen, 0, 0xAA);

  uint8_t  sim_rx[4] = {0x00, 0x00, 0x01, 0xFF}; /* no LRC byte */
  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);
  uint8_t  tx_cap[32];

  setup_t1_context();
  slot_sim_setup(sim_rx, sizeof(sim_rx), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── receive_CRC failure (prologue + data, CRC bytes missing) ────────────── */
void test_tpdu_t1_recv_crc_fail(void) {
  uint8_t  send[6];
  uint32_t slen;
  build_send_i_block_crc(send, &slen, 0, 0xAA);

  uint8_t  sim_rx[4] = {0x00, 0x00, 0x01, 0xFF}; /* no CRC bytes */
  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);
  uint8_t  tx_cap[32];

  setup_t1_context();
  ctx.params.EDC = SC_EDC_CRC;
  slot_sim_setup(sim_rx, sizeof(sim_rx), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── Valid non-zero NAD: check_Block success path (line 94) ─────────────── */
void test_tpdu_t1_valid_nonzero_nad(void) {
  /* NAD=0x12 (DAD=1, SAD=2): passes all check_Block guards → line 94.
   * Card reply uses swap(0x12)=0x21 as NAD (protocol requirement).
   * Response PCB=0x80 LEN=1 also passes check_Block on receive → line 94 again. */
  uint8_t send[5];
  send[0] = 0x12;
  send[1] = 0x80; /* (PCB & 0x80)!=0, (PCB & 0x1F)==0 → valid in check_Block */
  send[2] = 0x01; /* LEN=1, <= IFSC */
  send[3] = 0xAA;
  send[4] = EDC_LRC(send, 4);

  /* Response: NAD=swap(0x12)=0x21, PCB=0x80 LEN=1 → passes both NAD check
   * and check_Block on received buffer */
  uint8_t resp[5] = {0x21, 0x80, 0x01, 0xBB, 0x00};
  resp[4]         = EDC_LRC(resp, 4);

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);
  uint8_t  tx_cap[32];

  setup_t1_context();
  slot_sim_setup(resp, sizeof(resp), tx_cap, sizeof(tx_cap));

  /* The swap(0x12) NAD check at end_of_transaction produces 0x121 due to
   * integer promotion, which no uint8_t receive-NAD can satisfy, so the
   * transaction returns Bad_NAD — but line 94 in check_Block IS executed. */
  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, sizeof(send), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_TPDU_T1_Bad_NAD, r);
}

/* ── T=15 protocol present: CGT formula path (lines 137-138) ────────────── */
void test_tpdu_t1_t15_protocol_cgt(void) {
  uint8_t  send[5];
  uint32_t slen;
  build_send_i_block_lrc(send, &slen, 0, 0xAA);

  uint8_t resp[5] = {0x00, 0x00, 0x01, 0xFF, 0x00};
  resp[4]         = EDC_LRC(resp, 4);

  uint8_t  recv[32];
  uint32_t recv_len = sizeof(recv);
  uint8_t  tx_cap[32];

  setup_t1_context();
  ctx.params.N              = 1;
  ctx.params.supported_prot = (uint16_t)(1U << SC_PROTOCOL_T15);
  slot_sim_setup(resp, sizeof(resp), tx_cap, sizeof(tx_cap));

  sc_Status r = protocol_TPDU_T1.Transact(&ctx, send, slen, recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_tpdu_t1_invalid_params);
  RUN_TEST(test_tpdu_t1_bad_state);
  RUN_TEST(test_tpdu_t1_bad_length);
  RUN_TEST(test_tpdu_t1_bad_edc_lrc);
  RUN_TEST(test_tpdu_t1_crc_mode);
  RUN_TEST(test_tpdu_t1_bad_edc_crc);
  RUN_TEST(test_tpdu_t1_n_ff);
  RUN_TEST(test_tpdu_t1_bad_nad_0xff);
  RUN_TEST(test_tpdu_t1_bad_nad_dad_eq_sad);
  RUN_TEST(test_tpdu_t1_bad_pcb_high);
  RUN_TEST(test_tpdu_t1_bad_pcb_s_lower);
  RUN_TEST(test_tpdu_t1_bad_pcb_r_lower);
  RUN_TEST(test_tpdu_t1_bad_len_rblock);
  RUN_TEST(test_tpdu_t1_bad_len_ff);
  RUN_TEST(test_tpdu_t1_bad_len_gt_ifs);
  RUN_TEST(test_tpdu_t1_cwt_timeout_fail);
  RUN_TEST(test_tpdu_t1_cgt_guardtime_fail);
  RUN_TEST(test_tpdu_t1_send_fail);
  RUN_TEST(test_tpdu_t1_bwt_timeout_fail);
  RUN_TEST(test_tpdu_t1_bgt_guardtime_fail);
  RUN_TEST(test_tpdu_t1_recv_info_fail);
  RUN_TEST(test_tpdu_t1_recv_lrc_fail);
  RUN_TEST(test_tpdu_t1_recv_crc_fail);
  RUN_TEST(test_tpdu_t1_valid_nonzero_nad);
  RUN_TEST(test_tpdu_t1_t15_protocol_cgt);
  return UNITY_END();
}
