#include "unity.h"
#include "smartcard_config.h"
#include "sc_defs.h"
#include "sc_context.h"
#include "protocols.h"
#include "slot_sim.h"

static sc_context_t ctx;

static void setup_t0_context(void)
{
    iso_params_init(&ctx.params);
    ctx.params.state    = sc_state_active_on_t0;
    ctx.params.F        = SC_Fd;
    ctx.params.D        = SC_Dd;
    ctx.params.Fi       = SC_Fd;
    ctx.params.Di       = SC_Dd;
    ctx.params.fmax     = SC_fmaxd;
    ctx.params.frequency= SC_fmaxd;
    ctx.params.WI       = ATR_DEFAULT_WI;
    ctx.params.N        = ATR_DEFAULT_N;
    ctx.slot = &hslot_sim;
}

void setUp(void)    {}
void tearDown(void) {}

/* ── Case 1: no data in, no data out ─────────────────────────────────────── */
void test_apdu_t0_case1(void)
{
    /* APDU: CLA=00 INS=20 P1=00 P2=00 (4 bytes, Case 1) */
    uint8_t apdu[] = { 0x00, 0x20, 0x00, 0x00 };
    /* Card: SW1=0x90, SW2=0x00 */
    static const uint8_t card_resp[] = { 0x90, 0x00 };
    uint8_t tx_cap[32];
    slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
    setup_t0_context();

    uint8_t recv[16];
    uint32_t recv_len = sizeof(recv);

    sc_Status r = protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

    TEST_ASSERT_EQUAL(sc_Status_Success, r);
    TEST_ASSERT_EQUAL(2, recv_len);
    TEST_ASSERT_EQUAL_HEX8(0x90, recv[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, recv[1]);
}

/* ── Case 2S: receive Le bytes (Le=2) ────────────────────────────────────── */
void test_apdu_t0_case2s(void)
{
    /* APDU: READ BINARY, read 2 bytes */
    uint8_t apdu[] = { 0x00, 0xB0, 0x00, 0x00, 0x02 };
    /* Card: INS ACK, then 2 data bytes, then SW */
    static const uint8_t card_resp[] = { 0xB0, 0xDE, 0xAD, 0x90, 0x00 };
    uint8_t tx_cap[32];
    slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
    setup_t0_context();

    uint8_t recv[16];
    uint32_t recv_len = sizeof(recv);

    sc_Status r = protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

    TEST_ASSERT_EQUAL(sc_Status_Success, r);
    TEST_ASSERT_EQUAL(4, recv_len); /* 2 data + 2 SW */
    TEST_ASSERT_EQUAL_HEX8(0xDE, recv[0]);
    TEST_ASSERT_EQUAL_HEX8(0xAD, recv[1]);
    TEST_ASSERT_EQUAL_HEX8(0x90, recv[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, recv[3]);
}

/* ── Case 3S: send Nc=3 bytes ────────────────────────────────────────────── */
void test_apdu_t0_case3s(void)
{
    /* APDU: UPDATE BINARY, write 3 bytes */
    uint8_t apdu[] = { 0x00, 0xD6, 0x00, 0x00, 0x03, 0xAA, 0xBB, 0xCC };
    /* Card: INS ACK → send data, then SW */
    static const uint8_t card_resp[] = { 0xD6, 0x90, 0x00 };
    uint8_t tx_cap[32];
    slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
    setup_t0_context();

    uint8_t recv[16];
    uint32_t recv_len = sizeof(recv);

    sc_Status r = protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

    TEST_ASSERT_EQUAL(sc_Status_Success, r);
    TEST_ASSERT_EQUAL(2, recv_len); /* SW only */
    /* Verify we sent header + data */
    TEST_ASSERT_EQUAL(8, slot_sim_get_ctx()->tx_pos);
    TEST_ASSERT_EQUAL_HEX8(0xAA, tx_cap[5]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, tx_cap[6]);
    TEST_ASSERT_EQUAL_HEX8(0xCC, tx_cap[7]);
}

/* ── Case 4S: send Nc=2 bytes, receive Ne=1 byte ─────────────────────────── */
void test_apdu_t0_case4s(void)
{
    /* APDU: CLA INS P1 P2 Lc=2 data[2] Le=1 */
    uint8_t apdu[] = { 0x00, 0x88, 0x00, 0x00, 0x02, 0x01, 0x02, 0x01 };
    /* Card: INS ACK for send, then SW */
    static const uint8_t card_resp[] = { 0x88, 0x90, 0x00 };
    uint8_t tx_cap[32];
    slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
    setup_t0_context();

    uint8_t recv[16];
    uint32_t recv_len = sizeof(recv);

    sc_Status r = protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

    TEST_ASSERT_EQUAL(sc_Status_Success, r);
}

/* ── 0x6C: wrong length, card specifies Na ───────────────────────────────── */
void test_apdu_t0_wrong_length_6c(void)
{
    /* READ BINARY asking 3 bytes, card says only 1 available (SW=6C 01) */
    uint8_t apdu[] = { 0x00, 0xB0, 0x00, 0x00, 0x03 };
    /* First response: 0x6C 0x01 (send again with Le=1)
     * Then: INS ACK, 1 data byte, 0x90 0x00 */
    static const uint8_t card_resp[] = { 0x6C, 0x01, 0xB0, 0xFF, 0x90, 0x00 };
    uint8_t tx_cap[32];
    slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
    setup_t0_context();

    uint8_t recv[16];
    uint32_t recv_len = sizeof(recv);

    sc_Status r = protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

    TEST_ASSERT_EQUAL(sc_Status_Success, r);
    TEST_ASSERT_EQUAL(3, recv_len); /* 1 data + SW */
    TEST_ASSERT_EQUAL_HEX8(0xFF, recv[0]);
}

/* ── 0x61: GET RESPONSE chaining ─────────────────────────────────────────── */
void test_apdu_t0_get_response_61(void)
{
    /* Case 2S: Le=2. Card says 0x61 0x02 → need GET RESPONSE */
    uint8_t apdu[] = { 0x00, 0xB0, 0x00, 0x00, 0x02 };
    /* First TPDU: send header, receive 0x61 0x02
     * Second TPDU (GET RESPONSE): send [CLA C0 00 00 02], receive ACK 0xC0, data 0xDE 0xAD, 0x90 0x00 */
    static const uint8_t card_resp[] = {
        0x61, 0x02,           /* SW1=0x61: use GET RESPONSE, SW2=len */
        0xC0,                 /* ACK for GET RESPONSE */
        0xDE, 0xAD,           /* response data */
        0x90, 0x00            /* final SW */
    };
    uint8_t tx_cap[64];
    slot_sim_setup(card_resp, sizeof(card_resp), tx_cap, sizeof(tx_cap));
    setup_t0_context();

    uint8_t recv[16];
    uint32_t recv_len = sizeof(recv);

    sc_Status r = protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

    TEST_ASSERT_EQUAL(sc_Status_Success, r);
    TEST_ASSERT_EQUAL(4, recv_len); /* 2 data + SW */
    TEST_ASSERT_EQUAL_HEX8(0xDE, recv[0]);
    TEST_ASSERT_EQUAL_HEX8(0xAD, recv[1]);
    TEST_ASSERT_EQUAL_HEX8(0x90, recv[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, recv[3]);
}

/* ── Malformed APDU ──────────────────────────────────────────────────────── */
void test_apdu_t0_malformed(void)
{
    /* 3 bytes is not a valid APDU */
    uint8_t apdu[] = { 0x00, 0x20, 0x00 };
    slot_sim_setup(NULL, 0, NULL, 0);
    setup_t0_context();

    uint8_t recv[16];
    uint32_t recv_len = sizeof(recv);

    sc_Status r = protocol_APDU_T0.Transact(&ctx, apdu, sizeof(apdu), recv, &recv_len);

    TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, r);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_apdu_t0_case1);
    RUN_TEST(test_apdu_t0_case2s);
    RUN_TEST(test_apdu_t0_case3s);
    RUN_TEST(test_apdu_t0_case4s);
    RUN_TEST(test_apdu_t0_wrong_length_6c);
    RUN_TEST(test_apdu_t0_get_response_61);
    RUN_TEST(test_apdu_t0_malformed);
    return UNITY_END();
}
