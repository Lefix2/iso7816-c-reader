/*
 * WBSLOT
 * WBSLOT Driver implementation of interface slotItf
 */

#ifndef WBSLOT_H_
#define WBSLOT_H_

#include "slot_itf.h"

/************************************************************************************
 * Public defines
 ************************************************************************************/
// Specify if the driver have to use cmsis_os or active wait
#define WBSLOT_USE_CMSISOS
#define WBSLOT_USE_DMA

/************************************************************************************
 * Public types
 ************************************************************************************/

/************************************************************************************
 * Public variables
 ************************************************************************************/
extern slot_itf_t hslot_WBSLOT;

/************************************************************************************
 * Public functions
 ************************************************************************************/
/* API functions */

/* Platform interrupt callbacks */
void WBSLOT_USART_IRQHandler(void);

#ifdef WBSLOT_USE_DMA
void WBSLOT_DMA_TX_IRQHandler(void);

void WBSLOT_DMA_RX_IRQHandler(void);
#endif

#endif /* WBSLOT_H_ */
