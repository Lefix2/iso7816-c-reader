#include "sc_debug.h"
/*
 * Protocol PPS
 * Expose API for Protocol and Parameters Selection
 */

#include <string.h>

#include "protocols.h"
#include "sc_defs.h"
#include "slot_itf.h"

/************************************************************************************
 * Private defines
 ************************************************************************************/

/************************************************************************************
 * Private variables
 ************************************************************************************/

/************************************************************************************
 * Private functions
 ************************************************************************************/

static uint8_t pps_getlen(uint8_t PPS0) {
  uint8_t len;

  /* PPSS + PPS0 + PCK */
  len = 3;
  /* PPS1 */
  len += (PPS0 & 0x10) >> 4;
  /* PPS2 */
  len += (PPS0 & 0x20) >> 5;
  /* PPS3 */
  len += (PPS0 & 0x40) >> 6;

  return len;
}

static sc_Status protocol_pps_transact(sc_context_t *context,
                                       const uint8_t *send_buffer,
                                       uint32_t      send_length,
                                       uint8_t      *receive_buffer,
                                       uint32_t     *receive_length) {
  sc_Status  ret;
  slot_itf_t slot;

  uint8_t  pps_lenght;
  uint32_t buffer_size = *receive_length;

  *receive_length = 0;

  SC_DBG_COMM("PPS >> ", (char *)send_buffer, send_length);

  if ((send_length < 2) || (context == (void *)0) ||
      (context->slot == (void *)0)) {
    return sc_Status_Invalid_Parameter;
  }
  slot = *(context->slot);

  if (context->params.state != sc_state_negociable) {
    return sc_Status_Bad_State;
  }

  if (send_buffer[PPSS_IDX] != 0xFF) {
    return sc_Status_PPS_Bad_PPSS;
  }
  pps_lenght = pps_getlen(send_buffer[PPS0_IDX]);

  if (buffer_size < pps_lenght) {
    return sc_Status_Buffer_To_Small;
  }

  /* Sending PPS */
  ret = slot.send_bytes(send_buffer, pps_lenght);
  if (ret != sc_Status_Success) {
    return ret;
  }

  /* Receiving PPS response PPSS and PPS0*/
  ret = slot.receive_bytes(receive_buffer, 2);
  if (ret == sc_Status_Slot_Reception_Timeout) {
    return sc_Status_PPS_Unsuccessfull;
  } else if (ret != sc_Status_Success) {
    return ret;
  }

  *receive_length = pps_getlen(receive_buffer[PPS0_IDX]);

  /* Receiving remaining bytes*/
  ret = slot.receive_bytes(&receive_buffer[2], *receive_length - 2);
  if (ret != sc_Status_Success) {
    return ret;
  }

  if ((pps_lenght == *receive_length) &&
      memcmp(send_buffer, receive_buffer, pps_lenght)) {
    return sc_Status_PPS_Handshake_Error;
  }

  SC_DBG_COMM("PPS << ", (char *)receive_buffer, *receive_length);

  return sc_Status_Success;
}

/************************************************************************************
 * Public variables
 ************************************************************************************/

protocol_itf_t protocol_pps = {protocol_pps_transact};

/************************************************************************************
 * Public functions
 ************************************************************************************/
