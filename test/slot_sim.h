/*
 * slot_sim — configurable simulation slot for testing
 * Load rx bytes before calling protocol functions; inspect tx bytes after.
 */
#ifndef SLOT_SIM_H_
#define SLOT_SIM_H_

#include "slot_itf.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  /* RX side: bytes the slot will return when receive_byte/receive_bytes is
   * called */
  const uint8_t *rx_buf;
  uint32_t       rx_len;
  uint32_t       rx_pos;

  /* TX side: bytes captured when send_byte/send_bytes is called */
  uint8_t *tx_buf;
  uint32_t tx_cap;
  uint32_t tx_pos;

  /* Slot state */
  bool            present;
  bool            powered;
  uint32_t        frequency;
  uint32_t        F;
  uint32_t        D;
  uint32_t        timeout_etu;
  uint32_t        guardtime_etu;
  sc_convention_t convention;
  uint8_t         IFSD;
} slot_sim_ctx_t;

/**
 * Configure the global sim slot before a test.
 * @param rx      bytes to return from receive_*  (may be NULL if no rx
 * expected)
 * @param rx_len  length of rx
 * @param tx      buffer to capture sent bytes    (may be NULL if tx not
 * inspected)
 * @param tx_cap  capacity of tx buffer
 */
void slot_sim_setup(const uint8_t *rx,
                    uint32_t       rx_len,
                    uint8_t       *tx,
                    uint32_t       tx_cap);

/** Access the global context after a test to inspect sent bytes / state. */
slot_sim_ctx_t *slot_sim_get_ctx(void);

extern slot_itf_t hslot_sim;

#endif /* SLOT_SIM_H_ */
