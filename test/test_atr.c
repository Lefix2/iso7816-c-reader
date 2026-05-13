#include "unity.h"

#include "protocols.h"
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

/* ── atr_get_convention ──────────────────────────────────────────────────── */

void test_atr_get_convention_direct(void) {
  atr_t           atr;
  sc_convention_t conv;
  atr_init(&atr);
  atr.TS = 0x3B;
  TEST_ASSERT_EQUAL(sc_Status_Success, atr_get_convention(&atr, &conv));
  TEST_ASSERT_EQUAL(convention_direct, conv);
}

void test_atr_get_convention_reverse(void) {
  atr_t           atr;
  sc_convention_t conv;
  atr_init(&atr);
  atr.TS = 0x3F;
  TEST_ASSERT_EQUAL(sc_Status_Success, atr_get_convention(&atr, &conv));
  TEST_ASSERT_EQUAL(convention_reverse, conv);
}

void test_atr_get_convention_invalid(void) {
  atr_t           atr;
  sc_convention_t conv;
  atr_init(&atr);
  atr.TS = 0x00;
  TEST_ASSERT_EQUAL(sc_Status_ATR_Malformed, atr_get_convention(&atr, &conv));
}

/* ── atr_get_I (TB1) ─────────────────────────────────────────────────────── */

void test_atr_get_I_default(void) {
  atr_t    atr;
  uint32_t I;
  atr_init(&atr);
  TEST_ASSERT_EQUAL(sc_Status_Success, atr_get_I(&atr, &I));
  TEST_ASSERT_EQUAL(ATR_DEFAULT_I, I);
}

void test_atr_get_I_from_tb1(void) {
  atr_t    atr;
  uint32_t I;
  atr_init(&atr);
  /* TB1 bits [6:5] = 0b10 → index 2 → I = 100 mA */
  atr.T[0][ATR_INTERFACE_B].present = true;
  atr.T[0][ATR_INTERFACE_B].value   = 0x40;
  TEST_ASSERT_EQUAL(sc_Status_Success, atr_get_I(&atr, &I));
  TEST_ASSERT_EQUAL(100, I);
}

/* ── atr_get_P (TB1/TB2) ─────────────────────────────────────────────────── */

void test_atr_get_P_default(void) {
  atr_t   atr;
  uint8_t P;
  atr_init(&atr);
  TEST_ASSERT_EQUAL(sc_Status_Success, atr_get_P(&atr, &P));
  TEST_ASSERT_EQUAL(ATR_DEFAULT_P, P);
}

void test_atr_get_P_from_tb1(void) {
  atr_t   atr;
  uint8_t P;
  atr_init(&atr);
  /* TB1 low 5 bits = programming voltage, masked to 0x1F */
  atr.T[0][ATR_INTERFACE_B].present = true;
  atr.T[0][ATR_INTERFACE_B].value   = 0x55;
  TEST_ASSERT_EQUAL(sc_Status_Success, atr_get_P(&atr, &P));
  TEST_ASSERT_EQUAL(0x55 & 0x1F, P);
}

void test_atr_get_P_from_tb2(void) {
  atr_t   atr;
  uint8_t P;
  atr_init(&atr);
  atr.T[1][ATR_INTERFACE_B].present = true;
  atr.T[1][ATR_INTERFACE_B].value   = 0x20;
  TEST_ASSERT_EQUAL(sc_Status_Success, atr_get_P(&atr, &P));
  TEST_ASSERT_EQUAL(0x20, P);
}

/* ── atr_get_WI with TC2 present ─────────────────────────────────────────── */

void test_atr_get_wi_from_tc2(void) {
  atr_t   atr;
  uint8_t WI;
  atr_init(&atr);
  atr.T[1][ATR_INTERFACE_C].present = true;
  atr.T[1][ATR_INTERFACE_C].value   = 0x0F;
  TEST_ASSERT_EQUAL(sc_Status_Success, atr_get_WI(&atr, &WI));
  TEST_ASSERT_EQUAL(0x0F, WI);
}

void test_atr_get_wi_tc2_zero_is_malformed(void) {
  atr_t   atr;
  uint8_t WI;
  atr_init(&atr);
  atr.T[1][ATR_INTERFACE_C].present = true;
  atr.T[1][ATR_INTERFACE_C].value   = 0x00;
  TEST_ASSERT_EQUAL(sc_Status_ATR_Malformed, atr_get_WI(&atr, &WI));
}

/* ── sc_defs boundary tests ──────────────────────────────────────────────── */

void test_sc_defs_get_fi_reserved(void) {
  uint32_t F;
  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, get_iParams(7, 0, &F, NULL, NULL));
}

void test_sc_defs_get_di_reserved(void) {
  uint32_t D;
  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, get_iParams(0, 10, NULL, &D, NULL));
}

void test_sc_defs_get_fmax_reserved(void) {
  uint32_t fmax;
  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, get_iParams(7, 0, NULL, NULL, &fmax));
}

void test_sc_defs_get_i_reserved(void) {
  uint32_t I;
  /* Index 3: i_table[3]=0 → reserved */
  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, get_I(3, &I));
}

void test_sc_defs_get_min_etu_ns_valid(void) {
  /* Fi=9=512, Di=7=64, fmax=5000000 → etu = 10000*512 / (64*(5000000/100000))
   */
  uint32_t etu = get_min_etu_ns(9, 7);
  TEST_ASSERT_GREATER_THAN(0, etu);
}

void test_sc_defs_get_fi_out_of_bounds(void) {
  uint32_t F;
  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, get_iParams(17, 0, &F, NULL, NULL));
}

void test_sc_defs_get_di_out_of_bounds(void) {
  uint32_t D;
  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, get_iParams(0, 17, NULL, &D, NULL));
}

void test_sc_defs_get_fmax_out_of_bounds(void) {
  uint32_t fmax;
  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, get_iParams(17, 0, NULL, NULL, &fmax));
}

void test_sc_defs_get_i_out_of_bounds(void) {
  uint32_t I;
  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, get_I(17, &I));
}

void test_sc_defs_get_min_etu_ns_reserved_fi(void) {
  /* Fi index 7 → reserved (0) → get_Fi fails → get_min_etu_ns returns 0 */
  TEST_ASSERT_EQUAL(0, get_min_etu_ns(7, 1));
}

void test_sc_defs_get_min_etu_ns_reserved_di(void) {
  /* Di index 10 → reserved (0) → get_Di fails → returns 0 */
  TEST_ASSERT_EQUAL(0, get_min_etu_ns(1, 10));
}

/* ── T=1 specific accessors ──────────────────────────────────────────────── */

void test_atr_t1_ifs_present(void) {
  /* Build ATR with T[2][A] present and T[1][D].value = T1 (0x01) */
  atr_t   atr;
  uint8_t IFS;
  atr_init(&atr);
  atr.T[1][ATR_INTERFACE_D].present = true;
  atr.T[1][ATR_INTERFACE_D].value   = SC_PROTOCOL_T1;
  atr.T[2][ATR_INTERFACE_A].present = true;
  atr.T[2][ATR_INTERFACE_A].value   = 0x80; /* IFS=128, valid */
  TEST_ASSERT_EQUAL(sc_Status_Success, atr_T1_specific_get_IFS(&atr, &IFS));
  TEST_ASSERT_EQUAL(0x80, IFS);
}

void test_atr_t1_ifs_zero_is_malformed(void) {
  atr_t   atr;
  uint8_t IFS;
  atr_init(&atr);
  atr.T[1][ATR_INTERFACE_D].present = true;
  atr.T[1][ATR_INTERFACE_D].value   = SC_PROTOCOL_T1;
  atr.T[2][ATR_INTERFACE_A].present = true;
  atr.T[2][ATR_INTERFACE_A].value   = 0x00; /* IFS=0 → malformed */
  TEST_ASSERT_EQUAL(sc_Status_ATR_Malformed,
                    atr_T1_specific_get_IFS(&atr, &IFS));
}

void test_atr_t1_ifs_ff_is_malformed(void) {
  atr_t   atr;
  uint8_t IFS;
  atr_init(&atr);
  atr.T[1][ATR_INTERFACE_D].present = true;
  atr.T[1][ATR_INTERFACE_D].value   = SC_PROTOCOL_T1;
  atr.T[2][ATR_INTERFACE_A].present = true;
  atr.T[2][ATR_INTERFACE_A].value   = 0xFF; /* IFS=0xFF → malformed */
  TEST_ASSERT_EQUAL(sc_Status_ATR_Malformed,
                    atr_T1_specific_get_IFS(&atr, &IFS));
}

void test_atr_t1_cbwi_present(void) {
  atr_t   atr;
  uint8_t CWI, BWI;
  atr_init(&atr);
  atr.T[1][ATR_INTERFACE_D].present = true;
  atr.T[1][ATR_INTERFACE_D].value   = SC_PROTOCOL_T1;
  atr.T[2][ATR_INTERFACE_B].present = true;
  /* BWI=3, CWI=5 → value = (3<<4)|5 = 0x35 */
  atr.T[2][ATR_INTERFACE_B].value = 0x35;
  TEST_ASSERT_EQUAL(sc_Status_Success,
                    atr_T1_specific_get_CBWI(&atr, &CWI, &BWI));
  TEST_ASSERT_EQUAL(5, CWI);
  TEST_ASSERT_EQUAL(3, BWI);
}

void test_atr_t1_cbwi_bwi_too_large(void) {
  atr_t   atr;
  uint8_t CWI, BWI;
  atr_init(&atr);
  atr.T[1][ATR_INTERFACE_D].present = true;
  atr.T[1][ATR_INTERFACE_D].value   = SC_PROTOCOL_T1;
  atr.T[2][ATR_INTERFACE_B].present = true;
  atr.T[2][ATR_INTERFACE_B].value   = 0xA0; /* BWI=10 > 9 → malformed */
  TEST_ASSERT_EQUAL(sc_Status_ATR_Malformed,
                    atr_T1_specific_get_CBWI(&atr, &CWI, &BWI));
}

void test_atr_t1_edc_present(void) {
  atr_t   atr;
  uint8_t EDC;
  atr_init(&atr);
  atr.T[1][ATR_INTERFACE_D].present = true;
  atr.T[1][ATR_INTERFACE_D].value   = SC_PROTOCOL_T1;
  atr.T[2][ATR_INTERFACE_C].present = true;
  atr.T[2][ATR_INTERFACE_C].value   = 0x01; /* CRC */
  atr_T1_specific_get_EDC(&atr, &EDC);
  TEST_ASSERT_EQUAL(SC_EDC_CRC, EDC);
}

/* ── ATR: TB1 present (programming voltage / current) ───────────────────── */

void test_atr_with_tb1(void) {
  /* T0=0x20: TB1 present, K=0 */
  static const uint8_t raw[] = {0x3B, 0x20, 0x00};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);

  TEST_ASSERT_EQUAL(sc_Status_Success, r);
  TEST_ASSERT_TRUE(ctx.params.ATR.T[0][ATR_INTERFACE_B].present);
  TEST_ASSERT_EQUAL_HEX8(0x00, ctx.params.ATR.T[0][ATR_INTERFACE_B].value);
}

/* ── ATR: invalid params (send_length != 0) → Invalid_Parameter (line 50) ── */
void test_atr_invalid_params(void) {
  slot_sim_setup(NULL, 0, NULL, 0);
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 1, atr_buf, &atr_len);
  TEST_ASSERT_EQUAL(sc_Status_Invalid_Parameter, r);
}

/* ── ATR: set_timeout_etu fails → error propagated (line 64) ────────────── */
void test_atr_set_timeout_fail(void) {
  static const uint8_t raw[] = {0x3B, 0x00};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  slot_sim_get_ctx()->set_timeout_fail_countdown = 1;
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);
  TEST_ASSERT_EQUAL(sc_Status_Hardware_Error, r);
}

/* ── ATR: set_guardtime_etu fails → error propagated (line 67) ──────────── */
void test_atr_set_guardtime_fail(void) {
  static const uint8_t raw[] = {0x3B, 0x00};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  slot_sim_get_ctx()->set_guardtime_fail_countdown = 1;
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);
  TEST_ASSERT_EQUAL(sc_Status_Hardware_Error, r);
}

/* ── ATR: T0 receive timeout (line 90) ──────────────────────────────────── */
void test_atr_t0_receive_fail(void) {
  static const uint8_t raw[] = {0x3B}; /* only TS, no T0 */
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);
  TEST_ASSERT_EQUAL(sc_Status_Slot_Reception_Timeout, r);
}

/* ── ATR: interface byte buffer overflow (line 102) ─────────────────────── */
void test_atr_interface_bytes_overflow(void) {
  /* T0=0x70: TA1+TB1+TC1 present (no TD1) → len=3; buffer_size=4 < 2+3=5 */
  static const uint8_t raw[] = {0x3B, 0x70};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = 4;

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);
  TEST_ASSERT_EQUAL(sc_Status_Buffer_To_Small, r);
}

/* ── ATR: interface bytes receive timeout (line 107) ────────────────────── */
void test_atr_interface_bytes_receive_fail(void) {
  /* T0=0x10: TA1 present; only TS+T0 supplied, no TA1 → timeout */
  static const uint8_t raw[] = {0x3B, 0x10};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);
  TEST_ASSERT_EQUAL(sc_Status_Slot_Reception_Timeout, r);
}

/* ── ATR: too many TDi bytes (nb_T > 7) → Malformed (line 148) ──────────── */
void test_atr_too_many_td(void) {
  /* TS + T0=0x80 + 8 TDi=0x80 → nb_T reaches 8 > ATR_MAX_PROTOCOL=7 */
  static const uint8_t raw[] = {0x3B, 0x80, 0x80, 0x80, 0x80,
                                 0x80, 0x80, 0x80, 0x80, 0x80};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);
  TEST_ASSERT_EQUAL(sc_Status_ATR_Malformed, r);
}

/* ── ATR: historical bytes buffer overflow (line 159) ───────────────────── */
void test_atr_historical_bytes_overflow(void) {
  /* T0=0x0F: 15 historical bytes; buffer_size=10 → 2+15=17 > 10 */
  static const uint8_t raw[] = {0x3B, 0x0F};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = 10;

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);
  TEST_ASSERT_EQUAL(sc_Status_Buffer_To_Small, r);
}

/* ── ATR: historical bytes receive timeout (line 164) ───────────────────── */
void test_atr_historical_bytes_receive_fail(void) {
  /* T0=0x02: 2 historical bytes; only TS+T0 supplied → timeout */
  static const uint8_t raw[] = {0x3B, 0x02};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);
  TEST_ASSERT_EQUAL(sc_Status_Slot_Reception_Timeout, r);
}

/* ── ATR: TCK byte buffer overflow (line 175) ───────────────────────────── */
void test_atr_tck_buffer_overflow(void) {
  /* 3B 80 01 → TCK required; buffer_size=3 → 3+1=4 > 3 → overflow */
  static const uint8_t raw[] = {0x3B, 0x80, 0x01};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = 3;

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);
  TEST_ASSERT_EQUAL(sc_Status_Buffer_To_Small, r);
}

/* ── ATR: TCK receive timeout (line 180) ────────────────────────────────── */
void test_atr_tck_receive_fail(void) {
  /* 3B 80 01 → TCK required; only 3 bytes supplied → timeout on TCK */
  static const uint8_t raw[] = {0x3B, 0x80, 0x01};
  slot_sim_setup(raw, sizeof(raw), NULL, 0);
  setup_context();
  atr_len = sizeof(atr_buf);

  sc_Status r = protocol_atr.Transact(&ctx, NULL, 0, atr_buf, &atr_len);
  TEST_ASSERT_EQUAL(sc_Status_Slot_Reception_Timeout, r);
}

/* ── atr_get_N: TC1 present → returns TC1 value (line 270) ──────────────── */
void test_atr_get_n_tc1_present(void) {
  atr_t   atr;
  uint8_t N;
  atr_init(&atr);
  atr.T[0][ATR_INTERFACE_C].present = true;
  atr.T[0][ATR_INTERFACE_C].value   = 0x08;
  atr_get_N(&atr, &N);
  TEST_ASSERT_EQUAL(0x08, N);
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
  RUN_TEST(test_atr_with_tb1);
  RUN_TEST(test_atr_get_convention_direct);
  RUN_TEST(test_atr_get_convention_reverse);
  RUN_TEST(test_atr_get_convention_invalid);
  RUN_TEST(test_atr_get_I_default);
  RUN_TEST(test_atr_get_I_from_tb1);
  RUN_TEST(test_atr_get_P_default);
  RUN_TEST(test_atr_get_P_from_tb1);
  RUN_TEST(test_atr_get_P_from_tb2);
  RUN_TEST(test_atr_get_wi_from_tc2);
  RUN_TEST(test_atr_get_wi_tc2_zero_is_malformed);
  RUN_TEST(test_sc_defs_get_fi_reserved);
  RUN_TEST(test_sc_defs_get_di_reserved);
  RUN_TEST(test_sc_defs_get_fmax_reserved);
  RUN_TEST(test_sc_defs_get_i_reserved);
  RUN_TEST(test_sc_defs_get_min_etu_ns_valid);
  RUN_TEST(test_sc_defs_get_fi_out_of_bounds);
  RUN_TEST(test_sc_defs_get_di_out_of_bounds);
  RUN_TEST(test_sc_defs_get_fmax_out_of_bounds);
  RUN_TEST(test_sc_defs_get_i_out_of_bounds);
  RUN_TEST(test_sc_defs_get_min_etu_ns_reserved_fi);
  RUN_TEST(test_sc_defs_get_min_etu_ns_reserved_di);
  RUN_TEST(test_atr_t1_ifs_present);
  RUN_TEST(test_atr_t1_ifs_zero_is_malformed);
  RUN_TEST(test_atr_t1_ifs_ff_is_malformed);
  RUN_TEST(test_atr_t1_cbwi_present);
  RUN_TEST(test_atr_t1_cbwi_bwi_too_large);
  RUN_TEST(test_atr_t1_edc_present);
  RUN_TEST(test_atr_invalid_params);
  RUN_TEST(test_atr_set_timeout_fail);
  RUN_TEST(test_atr_set_guardtime_fail);
  RUN_TEST(test_atr_t0_receive_fail);
  RUN_TEST(test_atr_interface_bytes_overflow);
  RUN_TEST(test_atr_interface_bytes_receive_fail);
  RUN_TEST(test_atr_too_many_td);
  RUN_TEST(test_atr_historical_bytes_overflow);
  RUN_TEST(test_atr_historical_bytes_receive_fail);
  RUN_TEST(test_atr_tck_buffer_overflow);
  RUN_TEST(test_atr_tck_receive_fail);
  RUN_TEST(test_atr_get_n_tc1_present);
  return UNITY_END();
}
