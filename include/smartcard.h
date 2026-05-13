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

#ifndef SC_MAX_SLOTS
#define SC_MAX_SLOTS 2
#endif

/************************************************************************************
 * Public types
 ************************************************************************************/

/* Debug category bitmasks — pass OR-combination to smartcard_Set_Debug_Hook */
#define SC_DBG_CAT_GENERAL 0x01u
#define SC_DBG_CAT_ATR     0x02u
#define SC_DBG_CAT_PPS     0x04u
#define SC_DBG_CAT_APDU    0x08u
#define SC_DBG_CAT_TPDU    0x10u
#define SC_DBG_CAT_ALL     0x1Fu

/* hook(category, tag, data, len): data==NULL and len==0 for text-only messages */
typedef void (*sc_debug_hook_t)(uint8_t        category,
                                const char    *tag,
                                const uint8_t *data,
                                uint32_t       len);

/************************************************************************************
 * Public variables
 ************************************************************************************/

/************************************************************************************
 * Public functions
 ************************************************************************************/

sc_Status smartcard_Init(void);

void smartcard_Set_Debug_Hook(sc_debug_hook_t hook, uint8_t categories);

sc_Status smartcard_Register_slot(slot_itf_t *slot_interface, uint32_t *slot);

sc_Status smartcard_UnRegister_slot(uint32_t slot);

sc_Status smartcard_Power_On(uint32_t  slot,
                             uint8_t   preferred_protocol,
                             uint8_t  *atr,
                             uint32_t *atrlen,
                             uint8_t  *protocol);

sc_Status smartcard_Power_Off(uint32_t slot);

sc_Status smartcard_Xfer_Data(uint32_t       slot,
                              const uint8_t *send_buffer,
                              uint32_t       send_length,
                              uint8_t       *receive_buffer,
                              uint32_t      *receive_length);

bool smartcard_Is_Present(uint32_t slot);

bool smartcard_Is_Powered(uint32_t slot);

#endif /* __SMARTCARD_H__ */
