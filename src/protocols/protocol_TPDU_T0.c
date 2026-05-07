/*
 * Protocol TPDU T0
 * Expose API for Transport Protocol Data Unit in protocol T0
 */

#include "sc_defs.h"
#include "slot_itf.h"

#include "protocols.h"
#include "sc_debug.h"

/************************************************************************************
 * Private defines
 ************************************************************************************/

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#undef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define END_TRANSACTION(__ERR__)                                               \
  ret   = __ERR__;                                                             \
  state = TPDU_T0_end_of_transaction;                                          \
  break;

/************************************************************************************
 * Private variables
 ************************************************************************************/

/************************************************************************************
 * Private types
 ************************************************************************************/

typedef enum {
  TPDU_T0_start_of_transaction,
  TPDU_T0_send_header,
  TPDU_T0_receive_procedure_byte,
  TPDU_T0_transact_remaining_bytes,
  TPDU_T0_transact_remaining_byte,
  TPDU_T0_receive_SW2,
  TPDU_T0_end_of_transaction,
  TPDU_T0_exit
} TPDU_T0_state;

/************************************************************************************
 * Private functions
 ************************************************************************************/

static sc_Status protocol_TPDU_T0_transact(sc_context_t  *context,
                                           const uint8_t *send_buffer,
                                           uint32_t       send_length,
                                           uint8_t       *receive_buffer,
                                           uint32_t      *receive_length) {
  sc_Status   ret;
  slot_itf_t *slot;

  TPDU_T0_state state;
  bool          is_rcv         = false;
  uint32_t      len_to_receive = 0;
  uint32_t      len_to_send    = 0;
  uint32_t      len = 0, Ne = 0, Na = 0;
  uint8_t       proc_byte, SW1 = 0, SW2 = 0;
  uint8_t       INS = 0;
  uint32_t      WT, GT; /* Character times */

  slot            = context->slot;
  len_to_receive  = *receive_length;
  *receive_length = 0;
  len_to_send     = send_length;
  send_length     = 0;

  if (context == NULL || send_buffer == NULL || receive_buffer == NULL ||
      len_to_receive < 2) {
    return sc_Status_Invalid_Parameter;
  }

  if (context->params.state != sc_state_active_on_t0) {
    return sc_Status_Bad_State;
  }

  /* A TPDU only send or only receive data */
  if (len_to_send > 5 && len_to_receive > 2) {
    return sc_Status_TPDU_T0_Bad_Length;
  }

  /* Compute low level time checking 10.2 */
  /* WT = WI x 960 x Fi/f sec    and     etu = F/D x 1/f sec */
  /* WT = (WI x 960 x Fi x D)/F etu */

  WT = (uint32_t)((float)(context->params.WI * 960 * context->params.D) *
                  ((float)context->params.Fi / (float)context->params.F));
  if (context->params.N == 0xFF) {
    GT = 12;
  } else {
    // presence of T=15 protocol
    if (context->params.supported_prot & (0x0001 << SC_PROTOCOL_T15)) {
      GT = 12 + (context->params.Fi * context->params.N * context->params.D) /
                    (context->params.F * context->params.Di);
    } else {
      GT = 12 + context->params.N;
    }
  }

  if (context->params.D == 64) {
    /* Gt shall be at least 16 etu, 10.2 */
    GT = GT < 16 ? 16 : GT;
  }

  if ((ret = slot->set_timeout_etu(WT)) != sc_Status_Success) {
    return ret;
  }

  if ((ret = slot->set_guardtime_etu(GT)) != sc_Status_Success) {
    return ret;
  }

  ret   = sc_Status_Success;
  state = TPDU_T0_start_of_transaction;

  SC_DBG_COMM("T0 TPDU >> ", (char *)send_buffer, len_to_send);

  while (state != TPDU_T0_exit) {

    switch (state) {

    case TPDU_T0_start_of_transaction:

      if (len_to_send < 5) {
        END_TRANSACTION(sc_Status_TPDU_T0_Bad_Header);
      }

      if (len_to_receive > 2) {
        if (len_to_receive - 2 !=
            (send_buffer[P3_IDX] == 0 ? 256 : send_buffer[P3_IDX])) {
          END_TRANSACTION(sc_Status_TPDU_T0_Bad_Length);
        }
        is_rcv = true;
      } else {
        if (len_to_send - 5 != send_buffer[P3_IDX]) {
          END_TRANSACTION(sc_Status_TPDU_T0_Bad_Length);
        }
        is_rcv = false;
      }

      Ne = Na = len_to_receive - 2;
      if ((Ne > 0) &&
          (Ne != (send_buffer[P3_IDX] == 0 ? 256 : send_buffer[P3_IDX]))) {
        END_TRANSACTION(sc_Status_TPDU_T0_Bad_Header);
      }

      INS = send_buffer[INS_IDX];

      state = TPDU_T0_send_header;
      break;

    case TPDU_T0_send_header:

      ret = slot->send_bytes(send_buffer, TPDU_HEADER_SIZE);
      if (ret != sc_Status_Success) {
        END_TRANSACTION(ret);
      }

      send_length += TPDU_HEADER_SIZE;

      state = TPDU_T0_receive_procedure_byte;
      break;

    case TPDU_T0_receive_procedure_byte:
      /* ISO7816-3 10.3.3 */

      ret = slot->receive_byte(&proc_byte);
      if (ret != sc_Status_Success) {
        END_TRANSACTION(ret);
      }

      /* NULL byte, time extension request, wait for a proc byte */
      if (proc_byte == 0x60) {
        state = TPDU_T0_receive_procedure_byte;
        break;
      }

      /* SW1, wait for SW2 */
      if ((proc_byte & 0xF0) == 0x60 || (proc_byte & 0xF0) == 0x90) {
        SW1   = proc_byte;
        state = TPDU_T0_receive_SW2;
        break;
      }

      /* ACK */
      if (proc_byte == (INS) || proc_byte == (INS ^ 0x01)) // Deprecated
      {
        if (len_to_send > 0 || len_to_receive > 2) {
          state = TPDU_T0_transact_remaining_bytes;
          break;
        } else {
          /* No more data to transfer, wait for sw1 */
          state = TPDU_T0_receive_procedure_byte;
          break;
        }
      }

      /* ACK, transact one remaining byte */
      uint8_t inscmplt = (INS ^ 0xFF); // to prevent a bugged warning in gcc, we
                                       // put operation in tmp var
      if (proc_byte == inscmplt || proc_byte == (INS ^ 0xFE)) // Deprecated)
      {
        if (len_to_send > 0 || len_to_receive > 2) {
          state = TPDU_T0_transact_remaining_byte;
          break;
        } else {
          /* No more data to transfer, wait for sw1 */
          state = TPDU_T0_receive_procedure_byte;
          break;
        }
      }

      /* Invalid procedure byte, not supposed to be reach */
      END_TRANSACTION(sc_Status_TPDU_T0_Bad_Proc_byte);

      break;

    case TPDU_T0_transact_remaining_bytes:

      if (is_rcv) {
        if (*receive_length + 3 > len_to_receive) {
          END_TRANSACTION(sc_Status_TPDU_T0_Bad_Length);
        }

        len = len_to_receive - *receive_length - 2;

        ret = slot->receive_bytes(&receive_buffer[*receive_length], len);
        if (ret != sc_Status_Success) {
          END_TRANSACTION(ret);
        }
        *receive_length += len;

      } else {
        if (send_length + 1 > len_to_send) {
          END_TRANSACTION(sc_Status_TPDU_T0_Bad_Length);
        }

        len = len_to_send - send_length;

        ret = slot->send_bytes(&send_buffer[send_length], len);
        if (ret != sc_Status_Success) {
          END_TRANSACTION(ret);
        }
        send_length += len;
      }

      state = TPDU_T0_receive_procedure_byte;

      break;

    case TPDU_T0_transact_remaining_byte:

      if (is_rcv) {
        if (*receive_length + 3 > len_to_receive) {
          END_TRANSACTION(sc_Status_TPDU_T0_Bad_Length);
        }

        ret = slot->receive_byte(&receive_buffer[(*receive_length)++]);
        if (ret != sc_Status_Success) {
          END_TRANSACTION(ret);
        }
      } else {
        if (send_length + 1 > len_to_send) {
          END_TRANSACTION(sc_Status_TPDU_T0_Bad_Length);
        }

        ret = slot->send_byte(send_buffer[send_length++]);
        if (ret != sc_Status_Success) {
          END_TRANSACTION(ret);
        }
      }

      state = TPDU_T0_receive_procedure_byte;

      break;

    case TPDU_T0_receive_SW2:

      ret = slot->receive_byte(&SW2);
      if (ret != sc_Status_Success) {
        END_TRANSACTION(ret);
      }

      /* Wrong length, Na specified */
      if (SW1 == 0x6C) {
        Na                               = (SW2 == 0 ? 256 : SW2);
        len_to_receive                   = Na + 2;
        ((uint8_t *)send_buffer)[P3_IDX] = SW2;
        send_length                      = 0;
        is_rcv                           = true;
        state                            = TPDU_T0_send_header;
        break;
      }

      /* Set buffer length according to Case2S.3 */
      (*receive_length) = MIN(Na, (*receive_length));

      receive_buffer[(*receive_length)++] = SW1;
      receive_buffer[(*receive_length)++] = SW2;

      state = TPDU_T0_end_of_transaction;
      break;

    case TPDU_T0_end_of_transaction:
      if (ret == sc_Status_Success) {
        SC_DBG_COMM("T0 TPDU << ", (char *)receive_buffer, *receive_length);
      }

      state = TPDU_T0_exit;
      break;

    case TPDU_T0_exit:
      /* Not supposed to append */
      return sc_Status_Bad_State;
    }
  }

  return ret;
}

/************************************************************************************
 * Public variables
 ************************************************************************************/

protocol_itf_t protocol_TPDU_T0 = {protocol_TPDU_T0_transact};

/************************************************************************************
 * Public functions
 ************************************************************************************/
