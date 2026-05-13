#include <string.h>

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

/* ── Bad NAD in response (card replies with wrong addressing) ────────────── */
void test_apdu_t1_bad_nad(void) {
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[64];
  uint32_t rx_pos = 0;
  uint32_t blen;

  /* IFS response with correct NAD=0x00 */
  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* I-block response with bad NAD: 0x21 instead of 0x00 */
  uint8_t resp_data[] = {0xFF, 0x90, 0x00};
  sim_rx[rx_pos + 0]  = 0x21; /* wrong NAD */
  sim_rx[rx_pos + 1]  = 0x00; /* PCB I-block N(S)=0 */
  sim_rx[rx_pos + 2]  = sizeof(resp_data);
  memcpy(sim_rx + rx_pos + 3, resp_data, sizeof(resp_data));
  sim_rx[rx_pos + 3 + sizeof(resp_data)] =
      lrc_of(sim_rx + rx_pos, 3 + sizeof(resp_data));
  rx_pos += 4 + sizeof(resp_data);

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
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

/* ── CRC EDC mode: build_I_block/S_block/R_block with CRC-16 ─────────────── */
static void
build_s_ifs_response_crc(uint8_t *block, uint32_t *len, uint8_t ifsd) {
  block[0]     = 0x00;
  block[1]     = 0xE1; /* S(IFS response) */
  block[2]     = 0x01;
  block[3]     = ifsd;
  uint16_t crc = EDC_CRC(block, 4);
  block[4]     = (uint8_t)(crc >> 8);
  block[5]     = (uint8_t)(crc & 0xFF);
  *len         = 6;
}

static void build_i_block_crc(uint8_t       *block,
                              uint32_t      *len,
                              uint8_t        ns,
                              const uint8_t *data,
                              uint8_t        data_len) {
  block[0] = 0x00;
  block[1] = (uint8_t)(ns << 6);
  block[2] = data_len;
  if (data_len > 0)
    memcpy(block + 3, data, data_len);
  uint16_t crc        = EDC_CRC(block, 3 + data_len);
  block[3 + data_len] = (uint8_t)(crc >> 8);
  block[4 + data_len] = (uint8_t)(crc & 0xFF);
  *len                = 5 + data_len;
}

void test_apdu_t1_crc_mode(void) {
  /* CRC-16 mode: exercises build_I_block/S_block CRC paths and TPDU
   * receive_CRC state */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x02};

  uint8_t  sim_rx[128];
  uint32_t rx_pos = 0;
  uint32_t blen;

  build_s_ifs_response_crc(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  uint8_t resp_data[] = {0xDE, 0xAD, 0x90, 0x00};
  build_i_block_crc(sim_rx + rx_pos, &blen, 0, resp_data, sizeof(resp_data));
  rx_pos += blen;

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();
  ctx.params.EDC = SC_EDC_CRC;

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(4, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0xDE, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0xAD, recv[1]);
}

/* ── Card sends R(0) requesting retransmit: exercises process_R_block ────────
 */
void test_apdu_t1_card_retransmit(void) {
  /* Case 2S: Le=1. After IFS exchange, card sends R(Nr=0) requesting
   * retransmission of our I-block, then sends a good I-block response. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[128];
  uint32_t rx_pos = 0;
  uint32_t blen;

  /* IFS response */
  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* R(Nr=0): PCB=0x80, LEN=0 — card asks us to retransmit N(S)=0 */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = 0x80; /* PCB_R_BLOCK, N(R)=0 */
  sim_rx[rx_pos + 2] = 0x00;
  sim_rx[rx_pos + 3] = lrc_of(sim_rx + rx_pos, 3);
  rx_pos += 4;

  /* Good I-block response after retransmit: N(S)=0, data = [FF 90 00] */
  uint8_t resp_data[] = {0xFF, 0x90, 0x00};
  build_i_block_response(sim_rx + rx_pos, &blen, 0, resp_data,
                         sizeof(resp_data));
  rx_pos += blen;

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(3, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0xFF, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0x90, recv[1]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[2]);
}

/* ── Abort request from card: card sends S(ABORT request) ───────────────── */
void test_apdu_t1_abort_request(void) {
  /* After IFS exchange, card sends S(ABORT request); we echo S(ABORT response).
   * The subsequent empty I-block from card completes the transaction. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[128];
  uint32_t rx_pos = 0;
  uint32_t blen;

  /* IFS response */
  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* S(ABORT request): PCB=0xC2, LEN=0 */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = 0xC2; /* PCB_S_BLOCK | PCB_S_ABORT */
  sim_rx[rx_pos + 2] = 0x00;
  sim_rx[rx_pos + 3] = lrc_of(sim_rx + rx_pos, 3);
  rx_pos += 4;

  /* After abort response, card sends I-block with SW = [90 00] */
  uint8_t resp_data[] = {0x90, 0x00};
  build_i_block_response(sim_rx + rx_pos, &blen, 0, resp_data,
                         sizeof(resp_data));
  rx_pos += blen;

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  /* Abort request handled; transaction result forwarded */
  TEST_ASSERT_NOT_EQUAL(sc_Status_APDU_T1_Bad_Response, r);
}

/* ── Invalid params (tiny recv buffer) → Invalid_Parameter ──────────────── */
void test_apdu_t1_invalid_params(void) {
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);

  uint8_t  recv[16];
  uint32_t recv_len = 3; /* < 4 → Invalid_Parameter */

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, r);
}

/* ── get_IFSD failure → error propagated ─────────────────────────────────── */
void test_apdu_t1_get_ifsd_fail(void) {
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  setup_t1_context();
  slot_sim_setup(NULL, 0, NULL, 0);
  slot_sim_get_ctx()->get_ifsd_fail = 1;

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── Reader chaining: send data > IFSC triggers I_MORE bit ──────────────── */
void test_apdu_t1_reader_chaining(void) {
  /* 5-byte APDU with IFSC=2 forces reader to chain I-blocks */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x05};

  uint8_t  sim_rx[8];
  uint32_t blen;
  build_s_ifs_response(sim_rx, &blen, ATR_DEFAULT_IFS);

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, blen, tx_cap, sizeof(tx_cap));
  setup_t1_context();
  ctx.params.IFSC = 2; /* force chaining: 5 bytes > IFSC=2 */

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_APDU_T1_Bad_Response, r);
}

/* ── IFS request not answered with S-block (card sends I-block instead) ─── */
void test_apdu_t1_ifs_not_answered(void) {
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  /* Card sends I-block in response to our IFS S-request */
  uint8_t  sim_rx[8];
  uint32_t blen;
  uint8_t  resp_data[] = {0x90, 0x00};
  build_i_block_response(sim_rx, &blen, 0, resp_data, sizeof(resp_data));

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, blen, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_APDU_T1_Bad_Response, r);
}

/* ── Buffer too small for card response → Buffer_To_Small ───────────────── */
void test_apdu_t1_buffer_too_small(void) {
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[64];
  uint32_t rx_pos = 0;
  uint32_t blen;

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* Card sends 12 bytes of data — larger than recv buffer */
  uint8_t big_data[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  build_i_block_response(sim_rx + rx_pos, &blen, 0, big_data, sizeof(big_data));
  rx_pos += blen;

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[8];                 /* too small for 12 bytes */
  uint32_t recv_len = sizeof(recv); /* buffer_size=8, card sends 12 */

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Buffer_To_Small, r);
}

/* ── Invalid R-block (non-zero LEN) → error, retries, then Bad_Response ─── */
void test_apdu_t1_r_block_invalid_len(void) {
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[64];
  uint32_t rx_pos = 0;
  uint32_t blen;

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* Invalid R-block: LEN=1 (must be 0) */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = 0x80; /* R-block, ACK, N(R)=0 */
  sim_rx[rx_pos + 2] = 0x01; /* invalid: R-block must have LEN=0 */
  sim_rx[rx_pos + 3] = 0xFF;
  sim_rx[rx_pos + 4] = lrc_of(sim_rx + rx_pos, 4);
  rx_pos += 5;

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── Card sends S(IFS request) → reader updates IFSC and responds ────────── */
void test_apdu_t1_card_ifs_request(void) {
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[64];
  uint32_t rx_pos = 0;
  uint32_t blen;

  /* 1. S(IFS response) — reply to our IFS request (IFSD=32) */
  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* 2. S(IFS request from card, IFSC=80=0x50): NAD=00 PCB=C1 LEN=01 data=50 */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = 0xC1; /* PCB_S_BLOCK | PCB_S_IFS */
  sim_rx[rx_pos + 2] = 0x01;
  sim_rx[rx_pos + 3] = 0x50;
  sim_rx[rx_pos + 4] = lrc_of(sim_rx + rx_pos, 4);
  rx_pos += 5;

  /* 3. I-block response from card: N(S)=0, data=[0x90, 0x00] */
  uint8_t resp[] = {0x90, 0x00};
  build_i_block_response(sim_rx + rx_pos, &blen, 0, resp, sizeof(resp));
  rx_pos += blen;

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(0x50, ctx.params.IFSC); /* updated by IFS request */
}

/* ── CRC mode + EDC error → R-block built with CRC EDC ──────────────────── */
void test_apdu_t1_crc_with_edc_error(void) {
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x02};

  uint8_t  sim_rx[128];
  uint32_t rx_pos = 0;
  uint32_t blen;

  /* IFS response (CRC mode) */
  build_s_ifs_response_crc(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* Bad I-block: wrong CRC */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = 0x00;
  sim_rx[rx_pos + 2] = 0x02;
  sim_rx[rx_pos + 3] = 0xDE;
  sim_rx[rx_pos + 4] = 0xAD;
  sim_rx[rx_pos + 5] = 0x00; /* wrong CRC high byte */
  sim_rx[rx_pos + 6] = 0x00; /* wrong CRC low byte */
  rx_pos += 7;

  /* Valid I-block response after R(EDC_ERROR) retry */
  uint8_t resp_data[] = {0xDE, 0xAD, 0x90, 0x00};
  build_i_block_crc(sim_rx + rx_pos, &blen, 0, resp_data, sizeof(resp_data));
  rx_pos += blen;

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();
  ctx.params.EDC = SC_EDC_CRC;

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
}

/* ── Helpers for ISO 7816-3 Annex A scenario tests ──────────────────────── */

static void
build_r_block_sim(uint8_t *block, uint32_t *len, uint8_t nr, uint8_t pcb_err) {
  block[0] = 0x00;
  block[1] = (uint8_t)(PCB_R_BLOCK | (nr << 4) | pcb_err);
  block[2] = 0x00;
  block[3] = lrc_of(block, 3);
  *len     = 4;
}

static void build_s_block_sim(uint8_t       *block,
                              uint32_t      *len,
                              uint8_t        pcb_opts,
                              const uint8_t *data,
                              uint8_t        data_len) {
  block[0] = 0x00;
  block[1] = (uint8_t)(PCB_S_BLOCK | pcb_opts);
  block[2] = data_len;
  if (data_len && data)
    memcpy(block + 3, data, data_len);
  block[3 + data_len] = lrc_of(block, 3 + data_len);
  *len                = (uint32_t)(4 + data_len);
}

static void build_bad_i_block_response(uint8_t *block, uint32_t *len) {
  /* I(0,0) with wrong LRC — triggers TPDU_T1_Bad_EDC */
  block[0] = 0x00;
  block[1] = 0x00; /* I-block N(S)=0, no more */
  block[2] = 0x03;
  block[3] = 0xFF;
  block[4] = 0x90;
  block[5] = 0x00;
  block[6] = 0x00; /* wrong LRC (correct = 0x6C) */
  *len     = 7;
}

/* ── Sc 1: N(S) toggles across two consecutive Transact calls ────────────── */
void test_sc1_ns_toggle(void) {
  /* ISO 7816-3 Annex A Scenario 1: two I-block exchanges. Second uses N(S)=1
   * because Nd persists in context->params across calls. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x02};

  uint8_t  sim_rx[128];
  uint32_t rx_pos = 0, blen;
  uint8_t  sw[]   = {0x90, 0x00};

  /* Call 1: IFS → I(0,0) response */
  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;
  build_i_block_response(sim_rx + rx_pos, &blen, 0, sw, sizeof(sw));
  rx_pos += blen;

  /* Call 2: IFS → I(1,0) response (N(S)=1, Nc persisted from call 1) */
  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;
  build_i_block_response(sim_rx + rx_pos, &blen, 1, sw, sizeof(sw));
  rx_pos += blen;

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len;

  recv_len = sizeof(recv);
  sc_Status r1 =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_Success, r1);
  TEST_ASSERT_EQUAL(2, recv_len);

  recv_len = sizeof(recv);
  sc_Status r2 =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_Success, r2);
  TEST_ASSERT_EQUAL(2, recv_len);
}

/* ── Sc 5: reader chaining — full two-block chain with R-block ACK ──────────
 */
void test_sc5_reader_chaining_full(void) {
  /* IFSC=2 forces split of 4-byte APDU: I(0,1)[2B] → R(1) → I(1,0)[2B] →
   * I(0,0). Requires the HASMORE(Last_I.PCB) fix in process_R_block. */
  uint8_t apdu[] = {0x00, 0xC0, 0x00, 0x00}; /* 4-byte Case 1 APDU */

  uint8_t  sim_rx[64];
  uint32_t rx_pos = 0, blen;
  uint8_t  sw[]   = {0x90, 0x00};

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;
  /* Card ACKs first I-block and requests next: R(N(R)=1) */
  build_r_block_sim(sim_rx + rx_pos, &blen, 1, PCB_R_ACK);
  rx_pos += blen;
  /* Card sends final response after last I-block */
  build_i_block_response(sim_rx + rx_pos, &blen, 0, sw, sizeof(sw));
  rx_pos += blen;

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();
  ctx.params.IFSC = 2; /* force two-block chain: 4B APDU > IFSC */

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0x90, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[1]);
}

/* ── Sc 10: card requests retransmit twice before sending I-block ───────────
 */
void test_sc10_double_card_retransmit(void) {
  /* Card sends R(0) twice (failed to receive reader's I-block), then I(0,0).
   * Retries stay within limit (≤ 2), transaction succeeds. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[64];
  uint32_t rx_pos = 0, blen;
  uint8_t  sw[]   = {0x90, 0x00};

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;
  /* Card failed to receive I-block: sends R(N(R)=0) twice */
  build_r_block_sim(sim_rx + rx_pos, &blen, 0, PCB_R_ACK);
  rx_pos += blen;
  build_r_block_sim(sim_rx + rx_pos, &blen, 0, PCB_R_ACK);
  rx_pos += blen;
  /* Card finally received and responds */
  build_i_block_response(sim_rx + rx_pos, &blen, 0, sw, sizeof(sw));
  rx_pos += blen;

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len);
}

/* ── Sc 11: card sends wrong R direction during error recovery ───────────── */
void test_sc11_wrong_r_direction(void) {
  /* Reader sends I(0,0). Card response has bad EDC → reader sends R(0,EDC).
   * Card sends R(1) (wrong direction). Reader sends R(0,OTHER). Card sends
   * valid I(0,0). */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[64];
  uint32_t rx_pos = 0, blen;
  uint8_t  sw[]   = {0x90, 0x00};

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;
  build_bad_i_block_response(sim_rx + rx_pos, &blen);
  rx_pos += blen;
  /* Card sends R(1) instead of retransmitting I-block */
  build_r_block_sim(sim_rx + rx_pos, &blen, 1, PCB_R_ACK);
  rx_pos += blen;
  build_i_block_response(sim_rx + rx_pos, &blen, 0, sw, sizeof(sw));
  rx_pos += blen;

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len);
}

/* ── Sc 14: S(WTX request) erroneously received, card retransmits ────────── */
void test_sc14_wtx_bad_edc_retry(void) {
  /* After reader's I-block, card sends S(WTX request) with bad EDC. Reader
   * sends R(0,EDC). Card retransmits valid S(WTX request). Reader responds
   * S(WTX response). Card sends I(0,0). */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[128];
  uint32_t rx_pos   = 0, blen;
  uint8_t  wtx_mult = 2;
  uint8_t  sw[]     = {0x90, 0x00};

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* S(WTX request) with wrong LRC */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = PCB_S_BLOCK | PCB_S_WTX; /* 0xC3 */
  sim_rx[rx_pos + 2] = 0x01;
  sim_rx[rx_pos + 3] = wtx_mult;
  sim_rx[rx_pos + 4] = 0x00; /* wrong LRC (correct = 0xC0) */
  rx_pos += 5;

  /* S(WTX request) retransmitted correctly */
  build_s_block_sim(sim_rx + rx_pos, &blen, PCB_S_WTX, &wtx_mult, 1);
  rx_pos += blen;

  build_i_block_response(sim_rx + rx_pos, &blen, 0, sw, sizeof(sw));
  rx_pos += blen;

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len);
}

/* ── Sc 16: S(IFS request) erroneously received, card retransmits ────────── */
void test_sc16_card_ifs_bad_edc_retry(void) {
  /* After reader's I-block, card sends S(IFS request, IFSC=0x50) with bad EDC.
   * Reader sends R(0,EDC). Card retransmits valid S(IFS request). Reader
   * responds S(IFS response). Card sends I(0,0). IFSC updated to 0x50. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[128];
  uint32_t rx_pos   = 0, blen;
  uint8_t  new_ifsc = 0x50;
  uint8_t  sw[]     = {0x90, 0x00};

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* S(IFS request, 0x50) with wrong LRC */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = PCB_S_BLOCK | PCB_S_IFS; /* 0xC1 */
  sim_rx[rx_pos + 2] = 0x01;
  sim_rx[rx_pos + 3] = new_ifsc;
  sim_rx[rx_pos + 4] = 0x00; /* wrong LRC (correct = 0x90) */
  rx_pos += 5;

  /* S(IFS request, 0x50) retransmitted correctly */
  build_s_block_sim(sim_rx + rx_pos, &blen, PCB_S_IFS, &new_ifsc, 1);
  rx_pos += blen;

  build_i_block_response(sim_rx + rx_pos, &blen, 0, sw, sizeof(sw));
  rx_pos += blen;

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len);
  TEST_ASSERT_EQUAL(0x50, ctx.params.IFSC);
}

/* ── Sc 23: card chaining with R-block error in mid-chain ───────────────────
 */
void test_sc23_card_chain_r_error(void) {
  /* Reader sends I(0,0). Card sends I(0,1) MORE. Reader sends R(1) to ACK.
   * Card sends R(1) back (erroneously received reader's R(1)). Reader retries
   * with R(1,OTHER). Card sends I(1,0) to complete chain. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x04};

  uint8_t  sim_rx[128];
  uint32_t rx_pos = 0, blen;

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* Card sends I(0,1) MORE: data=[0xAA, 0xBB] */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = PCB_I_BLOCK | PCB_I_MORE; /* 0x20: N(S)=0, MORE */
  sim_rx[rx_pos + 2] = 0x02;
  sim_rx[rx_pos + 3] = 0xAA;
  sim_rx[rx_pos + 4] = 0xBB;
  sim_rx[rx_pos + 5] = lrc_of(sim_rx + rx_pos, 5);
  rx_pos += 6;

  /* Card sends R(1) — erroneously received reader's R(1) ACK */
  build_r_block_sim(sim_rx + rx_pos, &blen, 1, PCB_R_ACK);
  rx_pos += blen;

  /* Card sends I(1,0) to complete chain: data=[0xCC, 0xDD, 0x90, 0x00] */
  uint8_t chain_data[] = {0xCC, 0xDD, 0x90, 0x00};
  build_i_block_response(sim_rx + rx_pos, &blen, 1, chain_data,
                         sizeof(chain_data));
  rx_pos += blen;

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(6, recv_len); /* 0xAA 0xBB 0xCC 0xDD 0x90 0x00 */
  TEST_ASSERT_EQUAL_HEX8(0xAA, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0xBB, recv[1]);
  TEST_ASSERT_EQUAL_HEX8(0xCC, recv[2]);
  TEST_ASSERT_EQUAL_HEX8(0xDD, recv[3]);
}

/* ── Sc 34: EDC errors exhaust retries → resync succeeds → exchange ─────── */
void test_sc34_resync_success(void) {
  /* Three consecutive EDC errors drive retries > 2, triggering S(RESYNCH
   * request). Card responds S(RESYNCH response). State machine resets and
   * restarts the full IFS + I-block exchange, which succeeds. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[128];
  uint32_t rx_pos = 0, blen;
  uint8_t  sw[]   = {0x90, 0x00};

  /* Phase 1: IFS exchange */
  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* Phase 2: three bad I-blocks → retries 1, 2, >2 → resync */
  for (int i = 0; i < 3; i++) {
    build_bad_i_block_response(sim_rx + rx_pos, &blen);
    rx_pos += blen;
  }

  /* Phase 3: S(RESYNCH response) — PCB = 0xC0|0x20 = 0xE0 */
  build_s_block_sim(sim_rx + rx_pos, &blen, PCB_S_RESYNC | PCB_S_RESPONSE, NULL,
                    0);
  rx_pos += blen;

  /* Phase 4: fresh IFS exchange after resync */
  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* Phase 5: successful I-block exchange */
  build_i_block_response(sim_rx + rx_pos, &blen, 0, sw, sizeof(sw));
  rx_pos += blen;

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(2, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0x90, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[1]);
}

/* ── TPDU returns non-retryable error → END_TRANSACTION (line 296) ───────── */
void test_apdu_t1_hardware_error(void) {
  /* IFS exchange succeeds (send call 1). I-block send (call 2) fails with
   * Hardware_Error. APDU classifies it as "other error" → END_TRANSACTION. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[8];
  uint32_t blen;
  build_s_ifs_response(sim_rx, &blen, ATR_DEFAULT_IFS);

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, blen, tx_cap, sizeof(tx_cap));
  setup_t1_context();
  slot_sim_get_ctx()->send_fail_countdown = 2;

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_Hardware_Error, r);
}

/* ── Card sends I-block during reader chain → resync (lines 346-347) ─────── */
void test_apdu_t1_card_i_during_reader_chain(void) {
  /* IFSC=2 forces reader to send I(MORE). Card responds with I-block instead
   * of R-ACK. HASMORE(Last_I.PCB)==true && Last_I.sender==READER → resync. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x02};

  uint8_t  sim_rx[16];
  uint32_t rx_pos = 0, blen;

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* I-block from card (wrong: should be R-ACK) */
  uint8_t wrong_i[5] = {0x00, 0x00, 0x01, 0xAA, 0x00};
  wrong_i[4]         = lrc_of(wrong_i, 4);
  memcpy(sim_rx + rx_pos, wrong_i, sizeof(wrong_i));
  rx_pos += sizeof(wrong_i);

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();
  ctx.params.IFSC = 2;

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_APDU_T1_Bad_Response, r);
}

/* ── Card sends I with wrong N(S) after reader's last I (lines 356-359) ──── */
void test_apdu_t1_card_wrong_ns_after_reader(void) {
  /* Reader sends I(N(S)=0, no-MORE). Card replies with I(N(S)=1) but Nc=0
   * expected → R(OTHER_ERROR) retry, eventually Bad_Response. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[16];
  uint32_t rx_pos = 0, blen;

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* I-block with N(S)=1 (PCB bit6=1) — wrong when Nc=0 */
  uint8_t wrong_ns[5] = {0x00, 0x40, 0x01, 0xBB, 0x00};
  wrong_ns[4]         = lrc_of(wrong_ns, 4);
  memcpy(sim_rx + rx_pos, wrong_ns, sizeof(wrong_ns));
  rx_pos += sizeof(wrong_ns);

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── Card chain then wrong N(S) → resync (lines 407-408) ─────────────────── */
void test_apdu_t1_card_chain_wrong_ns(void) {
  /* Card sends I(0,MORE) → reader sends R-ACK.
   * Card sends I(0,no-MORE) again (wrong: Nc=1 expected) → resync. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x02};

  uint8_t  sim_rx[32];
  uint32_t rx_pos = 0, blen;

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* Card I(0, MORE) with data 0xAA */
  uint8_t i_more[6] = {0x00, 0x20, 0x01, 0xAA, 0x00};
  i_more[4]         = lrc_of(i_more, 4);
  memcpy(sim_rx + rx_pos, i_more, 5);
  rx_pos += 5;

  /* Card I(0, no-MORE) again — wrong N(S), Nc=1 expected */
  uint8_t i_wrong[5] = {0x00, 0x00, 0x01, 0xBB, 0x00};
  i_wrong[4]         = lrc_of(i_wrong, 4);
  memcpy(sim_rx + rx_pos, i_wrong, sizeof(i_wrong));
  rx_pos += sizeof(i_wrong);

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_NOT_EQUAL(sc_Status_Success, r);
}

/* ── Card chain overflows buffer (line 423) ─────────────────────────────────
 */
void test_apdu_t1_card_chain_buffer_overflow(void) {
  /* Card sends I(0,MORE)[2B] → reader R-ACK → card sends I(1,no-MORE)[7B].
   * Buffer size=8, receive_length=2 after first block, 2+7>8 → Buffer_To_Small.
   */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x04};

  uint8_t  sim_rx[32];
  uint32_t rx_pos = 0, blen;

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* Card I(0, MORE) with 2 data bytes */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = 0x20; /* N(S)=0, MORE */
  sim_rx[rx_pos + 2] = 0x02;
  sim_rx[rx_pos + 3] = 0xAA;
  sim_rx[rx_pos + 4] = 0xBB;
  sim_rx[rx_pos + 5] = lrc_of(sim_rx + rx_pos, 5);
  rx_pos += 6;

  /* Card I(1, no-MORE) with 7 data bytes (overflows buffer) */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = 0x40; /* N(S)=1, no MORE */
  sim_rx[rx_pos + 2] = 0x07;
  for (uint32_t i = 0; i < 7; i++)
    sim_rx[rx_pos + 3 + i] = (uint8_t)(i + 1);
  sim_rx[rx_pos + 10] = lrc_of(sim_rx + rx_pos, 10);
  rx_pos += 11;

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[8]; /* too small: 8 < 2+7 */
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_Buffer_To_Small, r);
}

/* ── Card chains three I-blocks: R-ACK sent after second (lines 431-434) ─── */
void test_apdu_t1_card_triple_chain(void) {
  /* I(0,MORE)→R-ACK→I(1,MORE)→R-ACK(lines 431-434)→I(0,no-MORE)→done */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x04};

  uint8_t  sim_rx[64];
  uint32_t rx_pos = 0, blen;

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* I(0, MORE): N(S)=0, MORE bit */
  uint8_t i0m[5] = {0x00, 0x20, 0x01, 0xAA, 0x00};
  i0m[4]         = lrc_of(i0m, 4);
  memcpy(sim_rx + rx_pos, i0m, sizeof(i0m));
  rx_pos += sizeof(i0m);

  /* I(1, MORE): N(S)=1, MORE bit */
  uint8_t i1m[5] = {0x00, 0x60, 0x01, 0xBB, 0x00};
  i1m[4]         = lrc_of(i1m, 4);
  memcpy(sim_rx + rx_pos, i1m, sizeof(i1m));
  rx_pos += sizeof(i1m);

  /* I(0, no-MORE): final block */
  uint8_t i0f[6] = {0x00, 0x00, 0x02, 0x90, 0x00, 0x00};
  i0f[5]         = lrc_of(i0f, 5);
  memcpy(sim_rx + rx_pos, i0f, sizeof(i0f));
  rx_pos += sizeof(i0f);

  uint8_t tx_cap[256];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(4, recv_len);
  TEST_ASSERT_EQUAL_HEX8(0xAA, recv[0]);
  TEST_ASSERT_EQUAL_HEX8(0xBB, recv[1]);
  TEST_ASSERT_EQUAL_HEX8(0x90, recv[2]);
  TEST_ASSERT_EQUAL_HEX8(0x00, recv[3]);
}

/* ── Helper: build sim_rx for 3 EDC errors then a resync response ───────────
 */
static uint32_t build_resync_setup(uint8_t       *sim_rx,
                                   const uint8_t *resync_response,
                                   uint32_t       resp_len) {
  uint32_t rx_pos = 0, blen;
  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;
  for (int i = 0; i < 3; i++) {
    build_bad_i_block_response(sim_rx + rx_pos, &blen);
    rx_pos += blen;
  }
  memcpy(sim_rx + rx_pos, resync_response, resp_len);
  rx_pos += resp_len;
  return rx_pos;
}

/* ── After resync, card responds with S-request (line 177 ISSREQ path) ──── */
void test_apdu_t1_resync_s_req_during_resync(void) {
  /* 3 EDC errors → resync(1). Card sends S(IFS request) in response to
   * S(RESYNC request). check_resync_pcb: ISSREQ→false (line 177) → resync. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  /* S(IFS request, IFSC=0x20): PCB=0xC1, ISSREQ → triggers line 177 */
  uint8_t s_ifs_req[5] = {0x00, 0xC1, 0x01, 0x20, 0x00};
  s_ifs_req[4]         = lrc_of(s_ifs_req, 4);

  uint8_t  sim_rx[128];
  uint32_t rx_pos = build_resync_setup(sim_rx, s_ifs_req, sizeof(s_ifs_req));

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_APDU_T1_Bad_Response, r);
}

/* ── After resync, card sends wrong S-response type (line 179) ───────────── */
void test_apdu_t1_resync_wrong_s_response(void) {
  /* 3 EDC errors → resync(1). Card sends S(IFS response) instead of
   * S(RESYNC response). check_resync_pcb: not ISSREQ, STYPE≠RESYNC → line 179.
   */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  /* S(IFS response): PCB=0xE1, ISSRESP but STYPE=IFS≠RESYNC → line 179 */
  uint8_t s_ifs_resp[5] = {0x00, 0xE1, 0x01, 0x20, 0x00};
  s_ifs_resp[4]         = lrc_of(s_ifs_resp, 4);

  uint8_t  sim_rx[128];
  uint32_t rx_pos = build_resync_setup(sim_rx, s_ifs_resp, sizeof(s_ifs_resp));

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_APDU_T1_Bad_Response, r);
}

/* ── After resync, S-request answered with I-block → resync (line 308) ───── */
void test_apdu_t1_resync_i_block_response(void) {
  /* 3 EDC errors → resync(1). Card sends I-block to S(RESYNC request).
   * resyncs≠0 → state = APDU_T1_resynch_request (line 308). */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  /* Valid I-block as response to our RESYNC request */
  uint8_t i_block[5] = {0x00, 0x00, 0x01, 0xFF, 0x00};
  i_block[4]         = lrc_of(i_block, 4);

  uint8_t  sim_rx[128];
  uint32_t rx_pos = build_resync_setup(sim_rx, i_block, sizeof(i_block));

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_APDU_T1_Bad_Response, r);
}

/* ── Bad IFS request (data=0) → R(OTHER_ERROR) (lines 587-590) ───────────── */
void test_apdu_t1_bad_ifs_request_zero(void) {
  /* Card sends S(IFS request) with IFSC=0 (invalid) → R(OTHER_ERROR). */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[16];
  uint32_t rx_pos = 0, blen;

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* S(IFS request, IFSC=0): data=0 → rcv_block[3]==0 → error */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = 0xC1; /* PCB_S_BLOCK | PCB_S_IFS */
  sim_rx[rx_pos + 2] = 0x01;
  sim_rx[rx_pos + 3] = 0x00; /* zero IFSC value → triggers lines 587-590 */
  sim_rx[rx_pos + 4] = lrc_of(sim_rx + rx_pos, 4);
  rx_pos += 5;

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_APDU_T1_Bad_Response, r);
}

/* ── Bad ABORT request (LEN≠0) → R(OTHER_ERROR) (lines 603-606) ─────────── */
void test_apdu_t1_bad_abort_request_len(void) {
  /* Card sends S(ABORT request) with LEN=1 instead of 0 → error. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[16];
  uint32_t rx_pos = 0, blen;

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* S(ABORT request, LEN=1): LEN≠0 → triggers lines 603-606 */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = 0xC2; /* PCB_S_BLOCK | PCB_S_ABORT */
  sim_rx[rx_pos + 2] = 0x01;
  sim_rx[rx_pos + 3] = 0xFF;
  sim_rx[rx_pos + 4] = lrc_of(sim_rx + rx_pos, 4);
  rx_pos += 5;

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_APDU_T1_Bad_Response, r);
}

/* ── Bad WTX request (LEN=0) → R(OTHER_ERROR) (lines 618-621) ───────────── */
void test_apdu_t1_bad_wtx_request_len(void) {
  /* Card sends S(WTX request) with LEN=0 instead of 1 → error. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[16];
  uint32_t rx_pos = 0, blen;

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* S(WTX request, LEN=0): LEN≠1 → triggers lines 618-621 */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = 0xC3; /* PCB_S_BLOCK | PCB_S_WTX */
  sim_rx[rx_pos + 2] = 0x00;
  sim_rx[rx_pos + 3] = lrc_of(sim_rx + rx_pos, 3);
  rx_pos += 4;

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_APDU_T1_Bad_Response, r);
}

/* ── Card sends R(Nr==Ns) when card was last sender → resync (lines 525-526)─
 */
void test_apdu_t1_card_r_nr_eq_ns(void) {
  /* Card sends I(0,MORE) → reader sends R-ACK(1).
   * Card replies with R(Nr=0) whose Nr equals card's last I-block N(S)=0.
   * process_R_block: Last_I.sender==CARD, Nr==Ns → resync (lines 525-526). */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x02};

  uint8_t  sim_rx[32];
  uint32_t rx_pos = 0, blen;

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* Card I(0, MORE) */
  uint8_t i_more[5] = {0x00, 0x20, 0x01, 0xAA, 0x00};
  i_more[4]         = lrc_of(i_more, 4);
  memcpy(sim_rx + rx_pos, i_more, sizeof(i_more));
  rx_pos += sizeof(i_more);

  /* Card R(Nr=0, ACK): Nr=0 == last I-block N(S)=0 → lines 525-526 */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] =
      0x80; /* PCB_R_BLOCK, Nr=0, ACK — passes malformed check */
  sim_rx[rx_pos + 2] = 0x00;
  sim_rx[rx_pos + 3] = lrc_of(sim_rx + rx_pos, 3);
  rx_pos += 4;

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_APDU_T1_Bad_Response, r);
}

/* ── Card sends S(RESYNC request) → falls through to resync (lines 636-637)─ */
void test_apdu_t1_card_resync_request(void) {
  /* Card sends S(RESYNC request, PCB=0xC0) after reader's I-block.
   * S-block processing: not ISSRESP, STYPE=RESYNC: not IFS/ABORT/WTX →
   * falls through to "Bad state" (lines 636-637) → resync. */
  uint8_t apdu[] = {0x00, 0xB0, 0x00, 0x00, 0x01};

  uint8_t  sim_rx[16];
  uint32_t rx_pos = 0, blen;

  build_s_ifs_response(sim_rx + rx_pos, &blen, ATR_DEFAULT_IFS);
  rx_pos += blen;

  /* S(RESYNC request): PCB=0xC0, LEN=0 */
  sim_rx[rx_pos + 0] = 0x00;
  sim_rx[rx_pos + 1] = 0xC0; /* PCB_S_BLOCK | PCB_S_RESYNC (request) */
  sim_rx[rx_pos + 2] = 0x00;
  sim_rx[rx_pos + 3] = lrc_of(sim_rx + rx_pos, 3);
  rx_pos += 4;

  uint8_t tx_cap[512];
  slot_sim_setup(sim_rx, rx_pos, tx_cap, sizeof(tx_cap));
  setup_t1_context();

  uint8_t  recv[64];
  uint32_t recv_len = sizeof(recv);

  sc_Status r =
      protocol_APDU_T1.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);
  TEST_ASSERT_EQUAL(sc_Status_APDU_T1_Bad_Response, r);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_apdu_t1_single_exchange);
  RUN_TEST(test_apdu_t1_wtx);
  RUN_TEST(test_apdu_t1_chaining);
  RUN_TEST(test_apdu_t1_bad_edc);
  RUN_TEST(test_apdu_t1_resync_abort);
  RUN_TEST(test_apdu_t1_bad_nad);
  RUN_TEST(test_apdu_t1_bad_state);
  RUN_TEST(test_apdu_t1_crc_mode);
  RUN_TEST(test_apdu_t1_card_retransmit);
  RUN_TEST(test_apdu_t1_abort_request);
  RUN_TEST(test_apdu_t1_invalid_params);
  RUN_TEST(test_apdu_t1_get_ifsd_fail);
  RUN_TEST(test_apdu_t1_reader_chaining);
  RUN_TEST(test_apdu_t1_ifs_not_answered);
  RUN_TEST(test_apdu_t1_buffer_too_small);
  RUN_TEST(test_apdu_t1_r_block_invalid_len);
  RUN_TEST(test_apdu_t1_card_ifs_request);
  RUN_TEST(test_apdu_t1_crc_with_edc_error);
  /* ISO 7816-3 Annex A scenario coverage */
  RUN_TEST(test_sc1_ns_toggle);
  RUN_TEST(test_sc5_reader_chaining_full);
  RUN_TEST(test_sc10_double_card_retransmit);
  RUN_TEST(test_sc11_wrong_r_direction);
  RUN_TEST(test_sc14_wtx_bad_edc_retry);
  RUN_TEST(test_sc16_card_ifs_bad_edc_retry);
  RUN_TEST(test_sc23_card_chain_r_error);
  RUN_TEST(test_sc34_resync_success);
  RUN_TEST(test_apdu_t1_hardware_error);
  RUN_TEST(test_apdu_t1_card_i_during_reader_chain);
  RUN_TEST(test_apdu_t1_card_wrong_ns_after_reader);
  RUN_TEST(test_apdu_t1_card_chain_wrong_ns);
  RUN_TEST(test_apdu_t1_card_chain_buffer_overflow);
  RUN_TEST(test_apdu_t1_card_triple_chain);
  RUN_TEST(test_apdu_t1_resync_s_req_during_resync);
  RUN_TEST(test_apdu_t1_resync_wrong_s_response);
  RUN_TEST(test_apdu_t1_resync_i_block_response);
  RUN_TEST(test_apdu_t1_bad_ifs_request_zero);
  RUN_TEST(test_apdu_t1_bad_abort_request_len);
  RUN_TEST(test_apdu_t1_bad_wtx_request_len);
  RUN_TEST(test_apdu_t1_card_resync_request);
  RUN_TEST(test_apdu_t1_card_r_nr_eq_ns);
  return UNITY_END();
}
