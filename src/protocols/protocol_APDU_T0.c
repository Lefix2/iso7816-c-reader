/*
 * Protocol APDU T0
 * Expose API for Application Protocol Data Unit in protocol T0
 */

#include "protocols.h"
#include "sc_debug.h"
#include "sc_defs.h"
#include "slot_itf.h"
#include <string.h>

/************************************************************************************
 * Private defines
 ************************************************************************************/

#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#undef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define END_TRANSACTION(__ERR__)                                               \
  ret   = __ERR__;                                                             \
  state = APDU_T0_end_of_transaction;                                          \
  break;

/************************************************************************************
 * Private variables
 ************************************************************************************/

/************************************************************************************
 * Private types
 ************************************************************************************/

typedef enum {
  APDU_T0_start_of_transaction,
  APDU_T0_Case_1,
  APDU_T0_Case_2S,
  APDU_T0_Case_3S,
  APDU_T0_Case_4S,
  APDU_T0_Case_2E,
  APDU_T0_Case_3E,
  APDU_T0_Case_4E,
  APDU_T0_Send,
  APDU_T0_Receive,
  APDU_T0_Enveloppe,
  APDU_T0_GetResponse,
  APDU_T0_Exchange_TPDU,
  APDU_T0_end_of_transaction,
  APDU_T0_exit
} APDU_T0_state;

/************************************************************************************
 * Private functions
 ************************************************************************************/

static sc_Status APDU_T0_Decode(APDU_T0_state *state,
                                const uint8_t *buffer,
                                uint32_t       n,
                                uint32_t      *Nc,
                                uint32_t      *Ne) {

  uint8_t  C5;
  uint32_t C6C7, Le;

  /* 1 */
  if (n == 4) {
    (*Nc)    = 0;
    (*Ne)    = 0;
    (*state) = APDU_T0_Case_1;
    return sc_Status_Success;
  }

  C5 = buffer[C5_IDX];

  /* 2S */
  if (n == 5) {
    (*Nc)    = 0;
    (*Ne)    = C5 == 0 ? 256 : C5;
    (*state) = APDU_T0_Case_2S;
    return sc_Status_Success;
  }

  /* 2E or 3E or 4E */
  if (C5 == 0) {
    C6C7 = (buffer[C6_IDX] << 8) | (buffer[C7_IDX]);

    /* 2E */
    if (n == 7) {
      (*Nc)    = 0;
      (*Ne)    = C6C7 == 0 ? 65536 : C6C7;
      (*state) = APDU_T0_Case_2E;
      return sc_Status_Success;
    }
    /* 3E */
    if (n == 7 + C6C7) {
      (*Nc)    = C6C7;
      (*Ne)    = 0;
      (*state) = APDU_T0_Case_3E;
      return sc_Status_Success;
    }
    /* 4E */
    if (n == 9 + C6C7) {
      (*Nc)    = C6C7;
      Le       = (buffer[8 + C6C7 - 1] << 8) | (buffer[9 + C6C7 - 1]);
      (*Ne)    = (Le == 0 ? 65536 : Le);
      (*state) = APDU_T0_Case_4E;
      return sc_Status_Success;
    }
  } else {
    (*Nc) = C5;
    /* 3S */
    if (n == 5U + C5) {
      (*Ne)    = 0;
      (*state) = APDU_T0_Case_3S;
      return sc_Status_Success;
    }
    /* 4S */
    if (n == 6U + C5) {
      *Ne    = buffer[n - 1];
      *state = APDU_T0_Case_4S;
      return sc_Status_Success;
    }
  }
  return sc_Status_APDU_T0_Malformed;
}

static sc_Status protocol_APDU_T0_transact(sc_context_t *context,
                                           const uint8_t *send_buffer,
                                           uint32_t      send_length,
                                           uint8_t      *receive_buffer,
                                           uint32_t     *receive_length) {
  sc_Status     ret;   /* Return value */
  APDU_T0_state state; /* State of the T0 APDU transaction */

  uint8_t  P3;       /* Handle new value for P3 byte */
  uint8_t  SW1, SW2; /* Procedure bytes */
  uint8_t  TPDU_buffer[TPDU_HEADER_SIZE + 255]; /* Buffer for TPDU exchange */
  uint8_t  data_offset = 0;                     /* Offset of APDU data*/
  uint32_t snd_len     = 0, rcv_len; /* TPDU length to send and receive */

  uint32_t Nc = 0, Ne = 0, Nx = 0; /* Value of data to send/receive/expect */
  uint32_t len_to_send;            /* Size of APDU to be sent */
  uint32_t buffer_size;            /* Size of the input receive buffer */
  uint8_t *buff_addr = receive_buffer; /* Current buffer receive address */

  /* Initialize and verify parameters */
  buffer_size     = *receive_length;
  *receive_length = 0;
  len_to_send     = send_length;
  send_length     = 0;

  if (context == NULL || send_buffer == NULL || receive_buffer == NULL ||
      buffer_size < 4) {
    return sc_Status_Invalid_Parameter;
  }

  if (context->params.state != sc_state_active_on_t0) {
    return sc_Status_Bad_State;
  }

  if (len_to_send < 4) {
    return sc_Status_Invalid_Parameter;
  }

  /* Initialize transaction */
  ret   = sc_Status_Success;
  state = APDU_T0_start_of_transaction;

  dbg_buff_comm("T0 APDU >> ", (char *)send_buffer, len_to_send);

  while (state != APDU_T0_exit) {

    switch (state) {

    case APDU_T0_start_of_transaction:

      ret = APDU_T0_Decode(&state, send_buffer, len_to_send, &Nc, &Ne);
      if (ret != sc_Status_Success) {
        END_TRANSACTION(ret);
      }

      if (buffer_size < Ne + 2) {
        END_TRANSACTION(sc_Status_Buffer_To_Small);
      }

      /* Set Nx to max value, card can ask other size during transaction */
      Nx = 256;
      /* Not valid for cases 1 & 2 but not used */
      data_offset = 5;

      break;

    case APDU_T0_Case_1:
    case APDU_T0_Case_3S:
    case APDU_T0_Case_4S:
      len_to_send = Nc;
      state       = APDU_T0_Send;
      break;

    case APDU_T0_Case_2S:
    case APDU_T0_Case_2E:
      len_to_send = 0;
      state       = APDU_T0_Receive;
      break;

    case APDU_T0_Case_3E:
    case APDU_T0_Case_4E:
      data_offset = 7; /* Lc = 3 bytes */
      if (Nc < 256)
        state = APDU_T0_Case_4S;
      else
        state = APDU_T0_Enveloppe;
      break;

    case APDU_T0_Send:
      /* When Nc<256 */
      /* TPDU = CLA INS P1 P2 P3=Nc {Nc Data bytes} */
      memcpy(TPDU_buffer, send_buffer, 4);
      TPDU_buffer[P3_IDX] = Nc;

      /* Copy data if there's some */
      memcpy(TPDU_buffer + TPDU_HEADER_SIZE, send_buffer + data_offset, Nc);

      snd_len = TPDU_HEADER_SIZE + Nc;
      send_length += Nc;

      rcv_len = 2; /* SW1 SW2 */

      state = APDU_T0_Exchange_TPDU;

      break;

    case APDU_T0_Receive:
      /* When Ne <= 255 or first command of extended cases */
      memcpy(TPDU_buffer, send_buffer, 4);
      P3                  = MIN(256, Ne) == 256 ? 0 : MIN(256, Ne);
      TPDU_buffer[P3_IDX] = P3;

      snd_len = TPDU_HEADER_SIZE;

      rcv_len = MIN(256, Ne) + 2; /* Data + SW1 SW2 */

      state = APDU_T0_Exchange_TPDU;

      break;

    case APDU_T0_Enveloppe:
      /* When Nc>=256 */
      P3 = MIN(255, len_to_send - send_length);

      TPDU_buffer[CLA_IDX] = send_buffer[CLA_IDX];
      TPDU_buffer[INS_IDX] = INS_ENVELOPPE;
      TPDU_buffer[P1_IDX]  = 0x00;
      TPDU_buffer[P2_IDX]  = 0x00;
      TPDU_buffer[P3_IDX]  = P3;

      memcpy(TPDU_buffer + TPDU_HEADER_SIZE, send_buffer + send_length, P3);

      snd_len = TPDU_HEADER_SIZE + P3;
      send_length += P3;

      rcv_len = 2; /* SW1 SW2 */

      state = APDU_T0_Exchange_TPDU;

      break;

    case APDU_T0_GetResponse:
      /* When Ne > 255 */
      P3 = MIN(Nx, Ne - *receive_length) == 256 ? 0
                                                : MIN(Nx, Ne - *receive_length);

      TPDU_buffer[CLA_IDX] = send_buffer[CLA_IDX];
      TPDU_buffer[INS_IDX] = INS_GET_RESPONSE;
      TPDU_buffer[P1_IDX]  = 0x00;
      TPDU_buffer[P2_IDX]  = 0x00;
      TPDU_buffer[P3_IDX]  = P3;

      snd_len = TPDU_HEADER_SIZE;
      rcv_len = MIN(Nx, Ne - *receive_length) + 2; /* Data + SW1 SW2*/

      state = APDU_T0_Exchange_TPDU;

      break;

    case APDU_T0_Exchange_TPDU:

      ret = protocol_TPDU_T0.Transact(context, TPDU_buffer, snd_len, buff_addr,
                                      &rcv_len);
      if (ret != sc_Status_Success) {
        END_TRANSACTION(ret);
      }

      *receive_length += rcv_len - 2;
      buff_addr += rcv_len - 2;

      SW1 = *buff_addr;
      SW2 = *(buff_addr + 1);

      if ((SW1 & 0xF0) == 0x90) {
        /* 0x9000 */
        if (SW1 == 0x90 && SW2 == 0x00) {
          /* All data sent */
          if (send_length == len_to_send) {
            /* Remaining data to receive */
            if (*receive_length < Ne) {
              // 04/06/18 patch, 9000 indicate end of transaction anyway
            }
            /* Success of transaction */
            END_TRANSACTION(ret);
          }
          if (send_length < len_to_send) {
            if (Nc >= 256) {
              state = APDU_T0_Enveloppe;
              break;
            }
            if (Nc > 0) {
              state = APDU_T0_Receive;
              break;
            }
            /* Error, Data to send but Nc = 0 */
            END_TRANSACTION(sc_Status_Bad_State);
          }
          /* send_length Not supposed to be greater than len_to_send */
          END_TRANSACTION(sc_Status_Bad_State);
        }
        /* Transaction ended with 0x9XYZ code */
        END_TRANSACTION(ret);
      }

      if ((SW1 & 0xF0) == 0x60) {
        /* Note: 0x61 should not append in case 1 and 3, but it can,
         * handle it in an upper layer is more ISO compliant
         */
        if (SW1 == 0x61) {
          if (Ne - *receive_length == 0) {
            END_TRANSACTION(ret);
          }
          if (Ne - *receive_length > 0) {
            Nx    = SW2 == 0 ? 256 : SW2;
            state = APDU_T0_GetResponse;
            break;
          }
          END_TRANSACTION(sc_Status_Bad_State);
        }
        /* Transaction ended with 0x6XYZ code */
        END_TRANSACTION(ret);
      }

      /* Non ISO SW1 (!= 0x9 or 0x6)*/
      END_TRANSACTION(sc_Status_APDU_T0_SW_Error);

      break;

    case APDU_T0_end_of_transaction:

      if (ret == sc_Status_Success) {
        /* Add last Sw1 Sw2 */
        (*receive_length) += 2;

        dbg_buff_comm("T0 APDU << ", (char *)receive_buffer, *receive_length);
      }

      state = APDU_T0_exit;
      break;

    case APDU_T0_exit:
      /* Not supposed to append */
      return sc_Status_Bad_State;
    }
  }

  return ret;
}

protocol_itf_t protocol_APDU_T0 = {protocol_APDU_T0_transact};
