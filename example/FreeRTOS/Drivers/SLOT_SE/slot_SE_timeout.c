/*
 * slot_SE_timeout.c
 *
 *  Created on: Mar 29, 2018
 *      Author: root
 */

#include "stm32l4xx_hal.h"
#include "platform_config.h"

#include "sc_defs.h"
#include "slot_SE.h"

/* keep handle on smartcard peripheral to update error register if needed */
SMARTCARD_HandleTypeDef* sc_ref;

sc_Status		SE_timeout_Init				( SMARTCARD_HandleTypeDef* sc_handle )
{

	if(!sc_handle){
		return sc_Status_Invalid_Parameter;
	}

	sc_ref = sc_handle;

	SLOT_SE_TIM_CLK_ENABLE();

	/* Set timer in one pulse mode, update interrupt by overflow only, Disabled*/
	SLOT_SE_TIMEOUT_TIM->CR1 = TIM_CR1_OPM | TIM_CR1_URS;

	/* Clear Update flag */
	CLEAR_BIT(SLOT_SE_TIMEOUT_TIM->SR, TIM_SR_UIF);

	/* Enable interrupts on update event */
	SLOT_SE_TIMEOUT_TIM->DIER = TIM_DIER_UIE;

	/* Set prescaler to have 500 us timer */
	SLOT_SE_TIMEOUT_TIM->PSC = SLOT_SE_GetCLK()/2000 - 1;

	/* Set default WT value */
	SLOT_SE_TIMEOUT_TIM->ARR = SC_DEFAULT_WT_MS;

	/* Enable interrupt vector */
	HAL_NVIC_EnableIRQ(SLOT_SE_TIMEOUT_IRQn);
	HAL_NVIC_SetPriority(SLOT_SE_TIMEOUT_IRQn,5,0);

	return sc_Status_Success;
}

sc_Status		SE_timeout_set_WT_ms		( uint32_t WT_ms )
{
	if(WT_ms > 0x7FFF){
		return sc_Status_Invalid_Parameter;
	}

	/* Set new value in ARR register */
	SLOT_SE_TIMEOUT_TIM->ARR = WT_ms*2;
	/* Update counter by setting UG bit */
	SET_BIT(SLOT_SE_TIMEOUT_TIM->EGR, TIM_EGR_UG);

	return sc_Status_Success;
}

sc_Status		SE_timeout_get_WT_ms		( uint32_t* WT_ms )
{
	if(!WT_ms){
		return sc_Status_Invalid_Parameter;
	}

	*WT_ms = SLOT_SE_TIMEOUT_TIM->EGR/2;

	return sc_Status_Success;
}

sc_Status		SE_timeout_start			( void )
{
	/* Force timer to reload counter values */
	SET_BIT(SLOT_SE_TIMEOUT_TIM->EGR, TIM_EGR_UG);
	/* Enable peripheral */
	SET_BIT(SLOT_SE_TIMEOUT_TIM->CR1, TIM_CR1_CEN);

	return sc_Status_Success;
}

sc_Status		SE_timeout_rearm			( void )
{
	/* Reload counter by setting UG bit*/
	SET_BIT(SLOT_SE_TIMEOUT_TIM->EGR, TIM_EGR_UG);

	return sc_Status_Success;
}

sc_Status		SE_timeout_stop				( void )
{
	/* Disable peripheral */
	CLEAR_BIT(SLOT_SE_TIMEOUT_TIM->CR1, TIM_CR1_CEN);

	return sc_Status_Success;
}

sc_Status		SE_timeout_DeInit			( void )
{
	/* Disable peripheral */
	CLEAR_BIT(SLOT_SE_TIMEOUT_TIM->CR1, TIM_CR1_CEN);

	/* Disable peripheral CLK */
	SLOT_SE_TIM_CLK_DISABLE();
	return sc_Status_Success;
}


void			SE_timeout_TIM_IRQHandler	( void )
{
	/* Clear update event flag */
	SLOT_SE_TIMEOUT_TIM->SR &= ~TIM_SR_UIF;

	/* Update sc_handle error field */
	sc_ref->ErrorCode |= HAL_SMARTCARD_ERROR_RTO;

	/* Abort transaction */
	HAL_SMARTCARD_AbortReceive_IT(sc_ref);
}
