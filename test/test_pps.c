#include "protocols.h"
#include "sc_context.h"
#include "sc_defs.h"
#include "slot_sim.h"
#include "unity.h"

static sc_context_t ctx;

static void setup_context(void) {
  iso_params_init(&ctx.params);
  ctx.params.state = sc_state_negociable;
  ctx.slot         = &hslot_sim;
}

void setUp(void) {}
void tearDown(void) {}

/* ── PPS: card echoes back identical PPS (negotiation accepted) ─────────── */
void test_pps_echo_accepted(void) {
  /* PPS request: PPSS=FF, PPS0=0x10 (T=0, PPS1 present), PPS1=0x11, PCK */
  uint8_t pps_req[] = {0xFF, 0x10, 0x11, 0x00};
  /* PCK = XOR(FF ^ 10 ^ 11) = 0xFE */
  pps_req[3] = pps_req[0] ^ pps_req[1] ^ pps_req[2]; /* = 0xFE */

  /* Card echoes the same PPS back */
  uint8_t tx_cap[16];
  slot_sim_setup(pps_req, sizeof(pps_req), tx_cap, sizeof(tx_cap));
  setup_context();

  uint8_t  resp[PPS_MAX_LENGTH];
  uint32_t resp_len = sizeof(resp);

  sc_Status r =
      protocol_pps.Transact(&ctx, pps_req, sizeof(pps_req), resp, &resp_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(sizeof(pps_req), resp_len);
  /* Verify we sent the PPS request */
  TEST_ASSERT_EQUAL(sizeof(pps_req), slot_sim_get_ctx()->tx_pos);
  TEST_ASSERT_EQUAL_MEMORY(pps_req, tx_cap, sizeof(pps_req));
}

/* ── PPS: minimal PPS (no PPS1), card echoes back ────────────────────────── */
void test_pps_minimal_echo(void) {
  /* PPSS=FF, PPS0=0x00 (T=0, no PPS1/2/3), PCK=FF^00=FF */
  uint8_t pps_req[] = {0xFF, 0x00, 0xFF};

  uint8_t tx_cap[16];
  slot_sim_setup(pps_req, sizeof(pps_req), tx_cap, sizeof(tx_cap));
  setup_context();

  uint8_t  resp[PPS_MAX_LENGTH];
  uint32_t resp_len = sizeof(resp);

  sc_Status r =
      protocol_pps.Transact(&ctx, pps_req, sizeof(pps_req), resp, &resp_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_EQUAL(3, resp_len);
}

/* ── PPS: card returns different response (negotiation failed) ───────────── */
void test_pps_handshake_error(void) {
  uint8_t pps_req[] = {0xFF, 0x10, 0x11, 0xFE};
  /* Card responds with different PPS1 value */
  uint8_t pps_resp[] = {0xFF, 0x10, 0x12, 0xFD};

  uint8_t tx_cap[16];
  slot_sim_setup(pps_resp, sizeof(pps_resp), tx_cap, sizeof(tx_cap));
  setup_context();

  uint8_t  resp[PPS_MAX_LENGTH];
  uint32_t resp_len = sizeof(resp);

  sc_Status r =
      protocol_pps.Transact(&ctx, pps_req, sizeof(pps_req), resp, &resp_len);

  TEST_ASSERT_EQUAL(sc_Status_PPS_Handshake_Error, r);
}

/* ── PPS: card timeout (no response) ────────────────────────────────────── */
void test_pps_timeout(void) {
  uint8_t pps_req[] = {0xFF, 0x00, 0xFF};

  uint8_t tx_cap[16];
  /* Give no RX bytes → receive_bytes returns timeout */
  slot_sim_setup(NULL, 0, tx_cap, sizeof(tx_cap));
  setup_context();

  uint8_t  resp[PPS_MAX_LENGTH];
  uint32_t resp_len = sizeof(resp);

  sc_Status r =
      protocol_pps.Transact(&ctx, pps_req, sizeof(pps_req), resp, &resp_len);

  TEST_ASSERT_EQUAL(sc_Status_PPS_Unsuccessfull, r);
}

/* ── PPS: bad PPSS byte ──────────────────────────────────────────────────── */
void test_pps_bad_ppss(void) {
  uint8_t pps_req[] = {0x00, 0x00, 0x00}; /* wrong PPSS */

  slot_sim_setup(NULL, 0, NULL, 0);
  setup_context();

  uint8_t  resp[PPS_MAX_LENGTH];
  uint32_t resp_len = sizeof(resp);

  sc_Status r =
      protocol_pps.Transact(&ctx, pps_req, sizeof(pps_req), resp, &resp_len);

  TEST_ASSERT_EQUAL(sc_Status_PPS_Bad_PPSS, r);
}

/* ── PPS: bad state ──────────────────────────────────────────────────────── */
void test_pps_bad_state(void) {
  uint8_t pps_req[] = {0xFF, 0x00, 0xFF};

  slot_sim_setup(NULL, 0, NULL, 0);
  setup_context();
  ctx.params.state = sc_state_active_on_t0; /* wrong state */

  uint8_t  resp[PPS_MAX_LENGTH];
  uint32_t resp_len = sizeof(resp);

  sc_Status r =
      protocol_pps.Transact(&ctx, pps_req, sizeof(pps_req), resp, &resp_len);

  TEST_ASSERT_EQUAL(sc_Status_Bad_State, r);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_pps_echo_accepted);
  RUN_TEST(test_pps_minimal_echo);
  RUN_TEST(test_pps_handshake_error);
  RUN_TEST(test_pps_timeout);
  RUN_TEST(test_pps_bad_ppss);
  RUN_TEST(test_pps_bad_state);
  return UNITY_END();
}
