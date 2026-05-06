#include "maths/EDC.h"
#include "protocols.h"
#include "sc_context.h"
#include "sc_defs.h"
#include "slot_sim.h"
#include <string.h>
#include "unity.h"

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

static uint8_t lrc_of(const uint8_t *buf, uint32_t len) {
  return EDC_LRC((uint8_t *)buf, len);
}

static void build_s_ifs_response(uint8_t *block, uint32_t *len, uint8_t ifsd) {
  /* S(IFS response): NAD=00, PCB=E1, LEN=01, data=ifsd, LRC */
  block[0] = 0x00;
  block[1] = 0xE1; /* PCB_S_BLOCK | PCB_S_IFS | PCB_S_RESPONSE */
  block[2] = 0x01;
  block[3] = ifsd;
  block[4] = lrc_of(block, 4);
  *len     = 5;
}

static void build_i_block_response(uint8_t       *block,
                                   uint32_t      *len,
                                   uint8_t        ns,
                                   const uint8_t *data,
                                   uint8_t        data_len) {
  /* I-block response: NAD=00, PCB=(ns<<6), LEN=data_len, data..., LRC */
  block[0] = 0x00;
  block[1] = (uint8_t)(ns << 6); /* I-block, N(S)=ns, no more */
  block[2] = data_len;
  if (data_len > 0)
    memcpy(block + 3, data, data_len);
  block[3 + data_len] = lrc_of(block, 3 + data_len);
  *len                = 4 + data_len;
}

void setUp(void) {}
void tearDown(void) {}

/* ── Single I-block exchange (IFS then data) ─────────────────────────────── */
void test_apdu_t1_single_exchange(void) {
  /* APDU: CLA=00 INS=B0 P1=00 P2=00 Le=2 (Case 2S) */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x02};

  /* sim rx: IFS response + I-block response with 2 data bytes + SW */
  uint8_t  sim_rx[64];
  uint32_t rx_pos = 0;
  uint32_t blen;

  /* 1. IFS response (IFSD = ATR_DEFAULT_IFS = 32) */
  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* 2. I-block response: data = [0xDE, 0xAD, 0x90, 0x00] (2 data bytes + SW) */
  uint8_t resp_data[] = {0xDE, 0xAD, 0x90, 0x00};
  build_i_block_response(sim_rx + rx_pos, &blen, 0, resp_data, 4);
  rx_pos += blen;

  uint8_t tx_cap[128];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(4, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0xDE, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0xAD, recv[1]);
  TEST_ASSERT_EQUAL_HEX8(0x90, recv[2]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[3]);
}

/* ── WTX: card requests wait time extension ──────────────────────────────── */
void test_apdu_t1_wtx(void) {
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[128];
  uint32_t rx_pos = 0;
  uint32_t blen;

  /* IFS response */
  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* WTX request: S(WTX request) [00 C3 01 02 LRC] */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = 0xC3; /* PCB_S_BLOCK | PCB_S_WTX */
  sim_rx[rx_pos + 2] = 0x01;
  sim_rx[rx_pos + 3] = 0x02; /* WT multiplier = 2 */
  sim_rx[rx_pos + 4] = lrc_of(sim_rx + rx_pos, 4);
  rx_pos += 5;

  /* I-block response after WTX ack */
  uint8_t resp_data[] = {0xFF, 0x90, 0x00};
  build_i_block_response(sim_rx + rx_pos, &blen, 0, resp_data, 3);
  rx_pos += blen;

  uint8_t tx_cap[128];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(3, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0xFF, recv[0]);
}

/* ── Chaining: card sends data in two I-blocks ───────────────────────────── */
void test_apdu_t1_chaining(void) {
  /* Ask for 4 bytes; card sends 2+2 across two I-blocks */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x04};

  uint8_t  sim_rx[128];
  uint32_t rx_pos = 0;
  uint32_t blen;

  /* IFS response */
  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* First I-block from card: N(S)=0, more=1, data=[0xAA, 0xBB] */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = 0x20; /* PCB_I_BLOCK | PCB_I_MORE, N(S)=0 */
  sim_rx[rx_pos + 2] = 0x02;
  sim_rx[rx_pos + 3] = 0xAA;
  sim_rx[rx_pos + 4] = 0xBB;
  sim_rx[rx_pos + 5] = lrc_of(sim_rx + rx_pos, 5);
  rx_pos += 6;

  /* Second I-block from card: N(S)=1, more=0, data=[0xCC, 0xDD, 0x90, 0x00] */
  uint8_t blk2_data[] = {0xCC, 0xDD, 0x90, 0x00};
  build_i_block_response(sim_rx + rx_pos, &blen, 1, blk2_data, 4);
  rx_pos += blen;

  uint8_t tx_cap[128];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(6, recv_len); /* 4 data + SW */
  TEST_ASSERT_EQUAL_HEX8(0xAA, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0xBB, recv[1]);
  TEST_ASSERT_EQUAL_HEX8(0xCC, recv[2]);
  TEST_ASSERT_EQUAL_HEX8(0xDD, recv[3]);
}

/* ── EDC error recovery: card retransmits valid I-block after R(EDC_ERROR) ──
 */
void test_apdu_t1_bad_edc(void) {
  /* ISO 7816-3 11.6.3: on EDC error reader sends R(EDC_ERROR), card retransmits
   */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[128];
  uint32_t rx_pos = 0;
  uint32_t blen;

  /* IFS response */
  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* Bad I-block: LEN=3, data=[FF 90 00], wrong LRC (correct=0x6C, use 0x00) */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = 0x00; /* PCB I-block N(S)=0 */
  sim_rx[rx_pos + 2] = 0x03;
  sim_rx[rx_pos + 3] = 0xFF;
  sim_rx[rx_pos + 4] = 0x90;
  sim_rx[rx_pos + 5] = 0x00;
  sim_rx[rx_pos + 6] = 0x00; /* wrong LRC — should be 0x6C */
  rx_pos += 7;

  /* Card retransmits valid I-block after R(EDC_ERROR) */
  uint8_t resp_data[] = {0xFF, 0x90, 0x00};
  build_i_block_response(sim_rx + rx_pos, &blen, 0, resp_data, 3);
  rx_pos += blen;

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  /* Retry mechanism recovers — transaction succeeds */
  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(3, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0xFF, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0x90, recv[1]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[2]);
}

/* ── Resync abort: 3 failed resyncs → sc_Status_APDU_T1_Bad_Response ─────── */
void test_apdu_t1_resync_abort(void) {
  /* No sim RX data at all → every receive times out.
   * Expected path: 3 R-block retries → resync1 (3 retries) → resync2 (3
   * retries) → resync3 (3 retries) → resyncs > 2 → abort. With the fixed state
   * machine this must terminate, not loop infinitely. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  /* IFS response so the IFS S-block exchange succeeds first, then fail */
  uint8_t  sim_rx[8];
  uint32_t blen;
  build_s_ifs_response(sim_rx, &blen, ATR_DEFAULT_IFS);

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, blen, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_APDU_T1_Bad_Response, r);
}

/* ── Bad state ──────────────────────────────────────────────────────────────
 */
void test_apdu_t1_bad_state(void) {
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};
  slot_sim_setup(NULL, 0, NULL, 0);
  setup_t1_context();
  ctx.params.state = sc_state_active_on_t0; /* wrong */

  uint8_t  recv[16];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Bad_State, r);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_apdu_t1_single_exchange);
  RUN_TEST(test_apdu_t1_wtx);
  RUN_TEST(test_apdu_t1_chaining);
  RUN_TEST(test_apdu_t1_bad_edc);
  RUN_TEST(test_apdu_t1_resync_abort);
  RUN_TEST(test_apdu_t1_bad_state);
  return UNITY_END();
}
