/*
 * smartcard
 * API to manage an ISO7816 communication
 */

#ifndef __SMARTCARD_H__
#define __SMARTCARD_H__

#include "sc_status.h"
#include "slot_itf.h"

/************************************************************************************
 * Public defines
 ************************************************************************************/

#define SC_MAX_SLOTS 2

/************************************************************************************
 * Public types
 ************************************************************************************/

/************************************************************************************
 * Public variables
 ************************************************************************************/

/************************************************************************************
 * Public functions
 ************************************************************************************/

sc_Status smartcard_Init(void);

sc_Status smartcard_Register_slot(slot_itf_t *slot_interface, uint32_t *slot);

sc_Status smartcard_UnRegister_slot(uint32_t slot);

sc_Status smartcard_Power_On(uint32_t  slot,
                             uint8_t   preferred_protocol,
                             uint8_t  *atr,
                             uint32_t *atrlen,
                             uint8_t  *protocol);

sc_Status smartcard_Power_Off(uint32_t slot);

sc_Status smartcard_Xfer_Data(uint32_t  slot,
                              uint8_t  *send_buffer,
                              uint32_t  send_length,
                              uint8_t  *receive_buffer,
                              uint32_t *receive_length);

bool smartcard_Is_Present(uint32_t slot);

bool smartcard_Is_Powered(uint32_t slot);

#endif /* __SMARTCARD_H__ */
