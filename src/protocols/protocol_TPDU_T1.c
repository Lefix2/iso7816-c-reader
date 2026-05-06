/*
 * Protocol TPDU T1
 * Expose API for Transport Protocol Data Unit in protocol T1
 */

#include "maths/EDC.h"
#include "protocols.h"
#include "sc_debug.h"
#include "sc_defs.h"
#include "slot_itf.h"

/************************************************************************************
 * Private defines
 ************************************************************************************/

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#undef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define swap(byte) (((byte) >> 4) | ((byte) << 4))

#define END_TRANSACTION(__ERR__)                                               \
  ret   = __ERR__;                                                             \
  state = TPDU_T1_end_of_transaction;                                          \
  break;

/************************************************************************************
 * Private variables
 ************************************************************************************/

/************************************************************************************
 * Private types
 ************************************************************************************/

typedef enum {
  TPDU_T1_start_of_transaction,
  TPDU_T1_send_block,
  TPDU_T1_receive_prologue,
  TPDU_T1_receive_information,
  TPDU_T1_receive_LRC,
  TPDU_T1_receive_CRC,
  TPDU_T1_end_of_transaction,
  TPDU_T1_exit
} TPDU_T1_state;

/************************************************************************************
 * Private functions
 ************************************************************************************/

static sc_Status
check_Block(uint8_t NAD, uint8_t PCB, uint8_t LEN, uint8_t IFS) {
  if (NAD == 0x00) {
    return sc_Status_Success;
  }

  if ((NAD == 0xFF) || ((NAD & 0x88) != 0x00) ||
      (((NAD >> 4) & 0x0F) == (NAD & 0x0F))) {
    return sc_Status_TPDU_T1_Bad_NAD;
  }

  /* I-block */
  if ((PCB & 0x80) != 0x00) {
    if ((PCB & 0x1F) != 0x00) {
      return sc_Status_TPDU_T1_Bad_PCB;
    }
  } else {
    /* S-block */
    if ((PCB & 0x40) != 0x00) {
      if ((PCB & 0x1C) != 0x00) {
        return sc_Status_TPDU_T1_Bad_PCB;
      }
    }
    /* R-block */
    else {
      if ((PCB & 0x2C) != 0x00) {
        return sc_Status_TPDU_T1_Bad_PCB;
      }
      if (LEN != 0x00) {
        return sc_Status_TPDU_T1_Bad_LEN;
      }
    }
  }

  if (LEN == 0xFF) {
    return sc_Status_TPDU_T1_Bad_LEN;
  }

  if (LEN > IFS) {
    return sc_Status_TPDU_T1_Bad_LEN;
  }

  return sc_Status_Success;
}

static sc_Status protocol_TPDU_T1_transact(sc_context_t *context,
                                           uint8_t      *send_buffer,
                                           uint32_t      send_length,
                                           uint8_t      *receive_buffer,
                                           uint32_t     *receive_length) {
  sc_Status     ret;   /* Return value */
  slot_itf_t   *slot;  /* Reference on the slot interface */
  TPDU_T1_state state; /* State of the T0 APDU transaction */

  uint32_t len_to_send;        /* Indicated length of data to be sent */
  uint32_t len_to_receive = 0; /* Indicated length of data to be received */
  uint32_t CWT, CGT;           /* Character times */
  uint32_t BWT, BGT;           /* Block times */
  uint32_t buffer_size;        /* Size of the input receive buffer */

  uint16_t CRC;

  /* Initialize and verify parameters */
  slot            = context->slot;
  buffer_size     = *receive_length;
  *receive_length = 0;
  len_to_send     = send_length;
  send_length     = 0;

  if (context == NULL || send_buffer == NULL || receive_buffer == NULL ||
      buffer_size < 4) {
    return sc_Status_Invalid_Parameter;
  }

  if (context->params.state != sc_state_active_on_t1) {
    return sc_Status_Bad_State;
  }

  /* Compute low level time checking 11.4.3 */
  CWT = 11 + (0x01 << context->params.CWI);
  if (context->params.N == 0xFF) {
    CGT = 11;
  } else {
    // presence of T=15 protocol
    if (context->params.supported_prot & (0x0001 << SC_PROTOCOL_T15)) {
      CGT = 12 + (context->params.Fi * context->params.N * context->params.D) /
                     (context->params.F * context->params.Di);
    } else {
      CGT = 12 + context->params.N;
    }
  }

  BWT = 11 + (((0x01 << context->params.BWI) * 960 * ATR_DEFAULT_F) /
              context->params.F) *
                 context->params.D;
  BGT = 22;

  /* 11.6.2.3 */
  if (context->params.WTX > 1) {
    /* TODO in worst case, BWT*WTX can reach value over 32bit = 8021606400d*/
    BWT *= context->params.WTX;
    context->params.WTX = 0;
  }

  /* Initialize transaction */
  ret   = sc_Status_Success;
  state = TPDU_T1_start_of_transaction;

  dbg_buff_comm("T1 TPDU >> ", (char *)send_buffer, len_to_send);

  while (state != TPDU_T1_exit) {

    switch (state) {

    case TPDU_T1_start_of_transaction:

      if (check_Block(send_buffer[NAD_IDX], send_buffer[PCB_IDX],
                      send_buffer[LEN_IDX],
                      context->params.IFSC) != sc_Status_Success) {
        END_TRANSACTION(sc_Status_TPDU_T1_Malformed);
      }

      if (len_to_send != (3U + send_buffer[LEN_IDX] +
                          (context->params.EDC == SC_EDC_LRC ? 1U : 2U))) {
        END_TRANSACTION(sc_Status_TPDU_T1_Bad_Length);
      }

      ret = slot->set_timeout_etu(CWT);
      if (ret != sc_Status_Success) {
        END_TRANSACTION(ret);
      }

      ret = slot->set_guardtime_etu(CGT);
      if (ret != sc_Status_Success) {
        END_TRANSACTION(ret);
      }

      state = TPDU_T1_send_block;
      break;

    case TPDU_T1_send_block:

      ret = slot->send_bytes(send_buffer, len_to_send);
      if (ret != sc_Status_Success) {
        END_TRANSACTION(ret);
      }

      ret = slot->set_timeout_etu(BWT);
      if (ret != sc_Status_Success) {
        END_TRANSACTION(ret);
      }

      ret = slot->set_guardtime_etu(BGT);
      if (ret != sc_Status_Success) {
        END_TRANSACTION(ret);
      }

      state = TPDU_T1_receive_prologue;
      break;

    case TPDU_T1_receive_prologue:

      ret = slot->receive_bytes(receive_buffer, T1_PROLOGUE_SIZE);
      if (ret != sc_Status_Success) {
        END_TRANSACTION(ret);
      }

      *receive_length += 3;

      len_to_receive = receive_buffer[LEN_IDX];

      state = TPDU_T1_receive_information;
      break;

    case TPDU_T1_receive_information:

      ret =
          slot->receive_bytes(receive_buffer + *receive_length, len_to_receive);
      if (ret != sc_Status_Success) {
        END_TRANSACTION(ret);
      }

      *receive_length += len_to_receive;

      if (context->params.EDC == SC_EDC_LRC) {
        state = TPDU_T1_receive_LRC;
        break;
      }

      if (context->params.EDC == SC_EDC_CRC) {
        state = TPDU_T1_receive_CRC;
        break;
      }

      END_TRANSACTION(sc_Status_Bad_State);

      break;

    case TPDU_T1_receive_LRC:

      ret = slot->receive_byte(receive_buffer + *receive_length);
      if (ret != sc_Status_Success) {
        END_TRANSACTION(ret);
      }

      if (EDC_LRC(receive_buffer, *receive_length) !=
          receive_buffer[*receive_length]) {
        END_TRANSACTION(sc_Status_TPDU_T1_Bad_EDC);
      }

      (*receive_length)++;

      state = TPDU_T1_end_of_transaction;
      break;

    case TPDU_T1_receive_CRC:

      ret = slot->receive_bytes(receive_buffer + *receive_length, CRC_SIZE);
      if (ret != sc_Status_Success) {
        END_TRANSACTION(ret);
      }

      CRC = (receive_buffer[*receive_length] << 8) |
            receive_buffer[*receive_length + 1];

      if (EDC_CRC(receive_buffer, *receive_length) != CRC) {
        END_TRANSACTION(sc_Status_TPDU_T1_Bad_EDC);
      }

      state = TPDU_T1_end_of_transaction;
      break;

    case TPDU_T1_end_of_transaction:

      if (ret == sc_Status_Success) {
        if (receive_buffer[NAD_IDX] != swap(send_buffer[NAD_IDX])) {
          ret = sc_Status_TPDU_T1_Bad_NAD;
        }
      }

      if (ret == sc_Status_Success) {
        ret = check_Block(receive_buffer[NAD_IDX], receive_buffer[PCB_IDX],
                          receive_buffer[LEN_IDX], context->params.IFSD);
      }

      if (ret == sc_Status_Success) {
        dbg_buff_comm("T1 TPDU << ", (char *)receive_buffer, *receive_length);
      }

      state = TPDU_T1_exit;
      break;

    case TPDU_T1_exit:
      /* Not supposed to append */
      return sc_Status_Bad_State;
    }
  }

  return ret;
}

/************************************************************************************
 * Public variables
 ************************************************************************************/

protocol_itf_t protocol_TPDU_T1 = {protocol_TPDU_T1_transact};

/************************************************************************************
 * Public functions
 ************************************************************************************/
