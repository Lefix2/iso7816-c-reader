/*
 * slot_SE_timeout.h
 *
 *  Created on: Mar 29, 2018
 *      Author: ftrefou
 *      This file tend to patch smartcard mode
 *      of stm32l4 USART. Indeed, timeout is measured after the leading
 *      edge of the last received bit. But ISO7816 specify
 *      that timeout must start after the leading edge of the
 *      last received OR send char. Moreover, ATR specify a
 *      timeout starting after the reset line have been pulled
 *      high.
 */

#ifndef SLOTS_SE_SLOT_SE_TIMEOUT_H_
#define SLOTS_SE_SLOT_SE_TIMEOUT_H_

#include "sc_status.h"

/* API functions */
sc_Status		SE_timeout_Init				( SMARTCARD_HandleTypeDef* sc_handle );

sc_Status		SE_timeout_set_WT_ms		( uint32_t WT_ms );

sc_Status		SE_timeout_get_WT_ms		( uint32_t* WT_ms );

sc_Status		SE_timeout_start			( void );

sc_Status		SE_timeout_rearm			( void );

sc_Status		SE_timeout_stop				( void );

sc_Status		SE_timeout_DeInit			( void );


/* Platform interrupt functions */
void			SE_timeout_TIM_IRQHandler	( void );


#endif /* SLOTS_SE_SLOT_SE_TIMEOUT_H_ */
