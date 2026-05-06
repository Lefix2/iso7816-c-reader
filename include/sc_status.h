/*
 * Sc status
 * Definitions of smartcard status return codes
 */

#ifndef __SC_STATUS_H_
#define __SC_STATUS_H_

#include <stdint.h>

/************************************************************************************
 * Public defines
 ************************************************************************************/

/************************************************************************************
 * Public types
 ************************************************************************************/

typedef enum {
  sc_Status_Success = 0x0000,

  sc_Status_Init_Error,
  sc_Status_DeInit_Error,
  sc_Status_Buffer_To_Small,
  sc_Status_Invalid_Parameter,
  sc_Status_Increment_Error,
  sc_Status_Unsuported_feature,
  sc_Status_Hardware_Error,
  sc_Status_Bad_State,

  sc_Status_Bad_Slot,
  sc_status_os_error,

  sc_Status_Slot_Reception_Timeout,
  sc_Status_Slot_Framing_Error,
  sc_Status_Slot_Parity_Error,
  sc_Status_Slot_Busy_Ressource,

  sc_Status_ATR_Bad_TS,
  sc_Status_ATR_Malformed,

  sc_Status_PPS_Bad_PPSS,
  sc_Status_PPS_Bad_PPS0,
  sc_Status_PPS_Handshake_Error,
  sc_Status_PPS_Unsuccessfull,

  sc_Status_TPDU_T0_Bad_Header,
  sc_Status_TPDU_T0_Bad_INS,
  sc_Status_TPDU_T0_Bad_Proc_byte,
  sc_Status_TPDU_T0_Bad_Length,

  sc_Status_APDU_T0_Malformed,
  sc_Status_APDU_T0_SW_Error,

  sc_Status_TPDU_T1_Malformed,
  sc_Status_TPDU_T1_Bad_Length,
  sc_Status_TPDU_T1_Bad_NAD,
  sc_Status_TPDU_T1_Bad_PCB,
  sc_Status_TPDU_T1_Bad_LEN,
  sc_Status_TPDU_T1_Bad_EDC,

  sc_Status_APDU_T1_Bad_Response,

  sc_Status_Invalid_Error = 0xFFFF
} sc_Status;

/************************************************************************************
 * Public variables
 ************************************************************************************/

/************************************************************************************
 * Public functions
 ************************************************************************************/

static inline const uint8_t *sc_status_str(sc_Status status) {

  switch (status) {
  case sc_Status_Success:
    return (uint8_t *)"Success";
  case sc_Status_Init_Error:
    return (uint8_t *)"Init_Error";
  case sc_Status_DeInit_Error:
    return (uint8_t *)"DeInit_Error";
  case sc_Status_Buffer_To_Small:
    return (uint8_t *)"Buffer_To_Small";
  case sc_Status_Invalid_Parameter:
    return (uint8_t *)"Invalid_Parameter";
  case sc_Status_Increment_Error:
    return (uint8_t *)"Increment_Error";
  case sc_Status_Unsuported_feature:
    return (uint8_t *)"Unsuported_feature";
  case sc_Status_Hardware_Error:
    return (uint8_t *)"Hardware_Error";
  case sc_Status_Bad_State:
    return (uint8_t *)"Bad_State";

  case sc_Status_Slot_Reception_Timeout:
    return (uint8_t *)"Slot_Reception_Timeout";
  case sc_Status_Slot_Framing_Error:
    return (uint8_t *)"Slot_Framing_Error";
  case sc_Status_Slot_Parity_Error:
    return (uint8_t *)"Slot_Parity_Error";
  case sc_Status_Slot_Busy_Ressource:
    return (uint8_t *)"Slot_Busy_Ressource";

  case sc_Status_Bad_Slot:
    return (uint8_t *)"Bad_Slot";
  case sc_status_os_error:
    return (uint8_t *)"os_error";

  case sc_Status_ATR_Bad_TS:
    return (uint8_t *)"ATR_Bad_TS";
  case sc_Status_ATR_Malformed:
    return (uint8_t *)"ATR_Malformed";

  case sc_Status_PPS_Bad_PPSS:
    return (uint8_t *)"PPS_Bad_PPSS";
  case sc_Status_PPS_Bad_PPS0:
    return (uint8_t *)"PPS_Bad_PPS0";
  case sc_Status_PPS_Handshake_Error:
    return (uint8_t *)"PPS_Handshake_Error";
  case sc_Status_PPS_Unsuccessfull:
    return (uint8_t *)"PPS_Unsuccessfull";

  case sc_Status_TPDU_T0_Bad_Header:
    return (uint8_t *)"TPDU_T0_Bad_Header";
  case sc_Status_TPDU_T0_Bad_INS:
    return (uint8_t *)"TPDU_T0_Bad_INS";
  case sc_Status_TPDU_T0_Bad_Proc_byte:
    return (uint8_t *)"TPDU_T0_Bad_Proc_byte";
  case sc_Status_TPDU_T0_Bad_Length:
    return (uint8_t *)"TPDU_T0_Bad_lenght";

  case sc_Status_APDU_T0_Malformed:
    return (uint8_t *)"APDU_T0_Malformed";
  case sc_Status_APDU_T0_SW_Error:
    return (uint8_t *)"APDU_T0_SW_Error";

  case sc_Status_TPDU_T1_Malformed:
    return (uint8_t *)"TPDU_T1_Malformed";
  case sc_Status_TPDU_T1_Bad_Length:
    return (uint8_t *)"TPDU_T1_Bad_Length";
  case sc_Status_TPDU_T1_Bad_NAD:
    return (uint8_t *)"TPDU_T1_Bad_NAD";
  case sc_Status_TPDU_T1_Bad_PCB:
    return (uint8_t *)"TPDU_T1_Bad_PCB";
  case sc_Status_TPDU_T1_Bad_LEN:
    return (uint8_t *)"TPDU_T1_Bad_LEN";
  case sc_Status_TPDU_T1_Bad_EDC:
    return (uint8_t *)"TPDU_T1_Bad_EDC";

  case sc_Status_APDU_T1_Bad_Response:
    return (uint8_t *)"APDU_T1_Bad_Response";

  default:
    return (uint8_t *)"Invalid Status";
  }
}

#endif /* __SC_STATUS_H_ */
