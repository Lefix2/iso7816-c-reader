#include "unity.h"

#include "maths/EDC.h"

void setUp(void) {}
void tearDown(void) {}

/* ── LRC ─────────────────────────────────────────────────────────────────── */

void test_edc_lrc_empty(void) {
  TEST_ASSERT_EQUAL_HEX8(0x00, EDC_LRC(NULL, 0));
}

void test_edc_lrc_single_zero(void) {
  uint8_t buf[] = {0x00};
  TEST_ASSERT_EQUAL_HEX8(0x00, EDC_LRC(buf, 1));
}

void test_edc_lrc_single_nonzero(void) {
  uint8_t buf[] = {0xAB};
  TEST_ASSERT_EQUAL_HEX8(0xAB, EDC_LRC(buf, 1));
}

void test_edc_lrc_multi(void) {
  /* PPS: FF 60 00 00 → PCK = 0xFF ^ 0x60 ^ 0x00 ^ 0x00 = 0x9F */
  uint8_t buf[] = {0xFF, 0x60, 0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX8(0x9F, EDC_LRC(buf, 4));
}

void test_edc_lrc_self_check(void) {
  /* XOR of a sequence plus its LRC equals 0 */
  uint8_t buf[] = {0x01, 0x02, 0x04};
  uint8_t lrc   = EDC_LRC(buf, 3);
  uint8_t check = lrc ^ 0x01 ^ 0x02 ^ 0x04;
  TEST_ASSERT_EQUAL_HEX8(0x00, check);
}

/* ── CRC ─────────────────────────────────────────────────────────────────── */

void test_edc_crc_empty(void) {
  /* No bytes → CRC stays at initial value 0xFFFF */
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, EDC_CRC(NULL, 0));
}

void test_edc_crc_single_zero(void) {
  /* Precomputed: CRC-16 of [0x00] with init=0xFFFF, poly=0x8408 (reflected)
   * Step: crc = (0xFFFF>>8) ^ table[0xFFFF & 0xFF] = 0x00FF ^ 0x0F78 = 0x0F87 */
  uint8_t buf[] = {0x00};
  TEST_ASSERT_EQUAL_HEX16(0x0F87, EDC_CRC(buf, 1));
}

void test_edc_crc_two_zeros(void) {
  /* Precomputed: CRC-16 of [0x00, 0x00]
   * After 1st byte: 0x0F87
   * 2nd byte: crc = (0x0F87>>8) ^ table[0x0F87 & 0xFF] = 0x000F ^ 0xF0B7 = 0xF0B8 */
  uint8_t buf[] = {0x00, 0x00};
  TEST_ASSERT_EQUAL_HEX16(0xF0B8, EDC_CRC(buf, 2));
}

void test_edc_crc_consistency(void) {
  /* CRC of the same bytes computed two different ways should match */
  uint8_t a[] = {0x3B, 0x80, 0x01};
  uint8_t b[] = {0x3B, 0x80, 0x01};
  TEST_ASSERT_EQUAL_HEX16(EDC_CRC(a, 3), EDC_CRC(b, 3));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_edc_lrc_empty);
  RUN_TEST(test_edc_lrc_single_zero);
  RUN_TEST(test_edc_lrc_single_nonzero);
  RUN_TEST(test_edc_lrc_multi);
  RUN_TEST(test_edc_lrc_self_check);
  RUN_TEST(test_edc_crc_empty);
  RUN_TEST(test_edc_crc_single_zero);
  RUN_TEST(test_edc_crc_two_zeros);
  RUN_TEST(test_edc_crc_consistency);
  return UNITY_END();
}
