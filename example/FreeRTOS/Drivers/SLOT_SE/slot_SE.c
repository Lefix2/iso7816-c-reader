/*
 * slot_SE.c
 *
 *  Created on: 26 f�vr. 2018
 *      Author: ftref
 */
#include <sc_defs.h>
#include <slot_itf.h>
#include <stdbool.h>

#include "stm32l4xx_hal.h"
#include "platform_config.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "debug.h"

#include "slot_SE_timeout.h"
#include "slot_SE.h"

/* Private defines */
#define MIN_GT		11
#define MAX_GT		0xFF
#define MIN_WT		12
#define MAX_WT		0xFFFFFF
#define MIN_CWT		11
#define MAX_CWT		0xFFFFFF
#define MIN_BWT		11
#define MAX_BWT		0xFF

//#define USE_ACTIVE_WAIT



SMARTCARD_HandleTypeDef hsmartcard;

#ifndef USE_ACTIVE_WAIT

SemaphoreHandle_t sc_txrx_sem;

#endif


/* private variable members */
uint8_t		guardtime_etu;
uint32_t	timeout_etu;
uint32_t	timeout_ms;
uint32_t	current_F;
uint32_t	current_D;
uint32_t	current_f;
uint8_t		current_IFSD;


/* t_slot private functions prototypes */
sc_Status	SE_init					( void );

sc_Status	SE_deinit				( void );

sc_Status	SE_get_state			(bool* present, bool* powered);

sc_Status	SE_activate				( sc_class_t class );

sc_Status	SE_deactivate			( void );

sc_Status	SE_send_byte			( uint8_t byte );

sc_Status	SE_send_bytes			( uint8_t* ptr, uint32_t len );

sc_Status	SE_receive_byte			( uint8_t* byte );

sc_Status	SE_receive_bytes		( uint8_t* ptr, uint32_t len );

sc_Status	SE_set_frequency		( uint32_t frequency );

sc_Status	SE_get_frequency		( uint32_t* frequency );

sc_Status	SE_set_timeout_etu		( uint32_t timeout );

sc_Status	SE_get_timeout_etu		( uint32_t* timeout );

sc_Status	SE_set_guardtime_etu	( uint8_t guardtime );

sc_Status	SE_get_guardtime_etu	( uint8_t* guardtime );

sc_Status	SE_set_convention		( sc_convention_t convention );

sc_Status	SE_get_convention		( sc_convention_t* convention );

sc_Status	SE_set_F_D				( uint32_t F, uint32_t D );

sc_Status	SE_get_F_D				( uint32_t* F, uint32_t* D );

sc_Status	SE_set_IFSD				( uint8_t IFSD );

sc_Status	SE_get_IFSD				( uint8_t* IFSD );


slot_itf_t hslot_SE = {
	SE_init,
	SE_deinit,
	SE_get_state,
	SE_activate,
	SE_deactivate,
	SE_send_byte,
	SE_send_bytes,
	SE_receive_byte,
	SE_receive_bytes,
	SE_set_frequency,
	SE_get_frequency,
	SE_set_timeout_etu,
	SE_get_timeout_etu,
	SE_set_guardtime_etu,
	SE_get_guardtime_etu,
	SE_set_convention,
	SE_get_convention,
	SE_set_F_D,
	SE_get_F_D,
	SE_set_IFSD,
	SE_get_IFSD
};



/* Private functions declarations */

void enable_vcc( bool enable){

	HAL_GPIO_WritePin(	SLOT_SE_VDD_GPIO_Port,
						SLOT_SE_VDD_Pin,
						enable
						);
}

void enable_rst( bool enable){

	HAL_GPIO_WritePin(	SLOT_SE_RST_GPIO_Port,
						SLOT_SE_RST_Pin,
						!enable
						);
}

static uint32_t	compute_baudrate		(uint32_t F, uint32_t D){

	/* Baudrate = ( CLK / ( 2*Prescaler )) * (1 / ( F/D )) */
	return ((SLOT_SE_GetCLK()/2) * D)/(hsmartcard.Init.Prescaler * F);

}


/* slot_SE public function declaration */

sc_Status	SE_init					( void ){

	sc_Status ret;


#ifndef USE_ACTIVE_WAIT
	/* Init semaphore */
	if (sc_txrx_sem == 0){
		sc_txrx_sem = xSemaphoreCreateBinary();
	}
	if (sc_txrx_sem == 0){
		return sc_Status_Init_Error;
	}

#endif

	/* Init timeout measurement */
	if ( (ret = SE_timeout_Init(&hsmartcard)) != sc_Status_Success ){
		return ret;
	}

	timeout_ms		= SC_DEFAULT_WT_MS;
	timeout_etu		= SC_DEFAULT_WT_ETU;
	guardtime_etu	= 11;
	current_F		= SC_Fd;
	current_D		= SC_Dd;
	current_f		= 4000000;
	current_IFSD	= ATR_DEFAULT_IFS;

	GPIO_InitTypeDef GPIO_InitStruct;
	/* Peripheral clock enable */

	SLOT_SE_USART_CLK_ENABLE();
	SLOT_SE_PORT_CLK_ENABLE();

	/* PA8 (USART1_CK) : ISO7816_CLK_Pin */
	GPIO_InitStruct.Pin = SLOT_SE_CK_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = SLOT_SE_GPIO_AF;
	HAL_GPIO_Init(SLOT_SE_CK_GPIO_Port, &GPIO_InitStruct);

	/* PA9 (USART1_TX) : ISO7816_IO_Pin */
	GPIO_InitStruct.Pin = SLOT_SE_IO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = SLOT_SE_GPIO_AF;
	HAL_GPIO_Init(SLOT_SE_IO_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : ISO7816_RST_Pin */
	GPIO_InitStruct.Pin = SLOT_SE_RST_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(SLOT_SE_RST_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : ISO7816_VDD_Pin */
	GPIO_InitStruct.Pin = SLOT_SE_VDD_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(SLOT_SE_VDD_GPIO_Port, &GPIO_InitStruct);

	hsmartcard.Instance = SLOT_SE_USART;
	hsmartcard.Init.Mode = SMARTCARD_MODE_TX_RX;
	hsmartcard.Init.Prescaler = 10;
	hsmartcard.Init.BaudRate = compute_baudrate(current_F, current_D);
	hsmartcard.Init.WordLength = SMARTCARD_WORDLENGTH_9B;
	hsmartcard.Init.StopBits = SMARTCARD_STOPBITS_1_5;
	hsmartcard.Init.Parity = SMARTCARD_PARITY_EVEN;
	hsmartcard.Init.OneBitSampling = SMARTCARD_ONE_BIT_SAMPLE_DISABLE;
	hsmartcard.Init.NACKEnable = SMARTCARD_NACK_DISABLE;
	hsmartcard.Init.GuardTime = 0;
	hsmartcard.Init.TimeOutEnable = SMARTCARD_TIMEOUT_DISABLE;
	hsmartcard.Init.TimeOutValue = timeout_etu;
	hsmartcard.Init.BlockLength = 0;
	hsmartcard.Init.AutoRetryCount = 0;
	hsmartcard.AdvancedInit.AdvFeatureInit = SMARTCARD_ADVFEATURE_NO_INIT;

	if(HAL_SMARTCARD_Init(&hsmartcard) != HAL_OK){
		return sc_Status_Init_Error;
	}

	// Enable IRQ and set them under os irq and timeout irq
	HAL_NVIC_EnableIRQ(SLOT_SE_IRQn);
	HAL_NVIC_SetPriority(SLOT_SE_IRQn,5,1);

	SE_deactivate();

	return sc_Status_Success;
}

sc_Status	SE_deinit				( void )
{
	/* DeInit IOs */
	HAL_GPIO_DeInit(SLOT_SE_CK_GPIO_Port,  SLOT_SE_CK_Pin );
	HAL_GPIO_DeInit(SLOT_SE_IO_GPIO_Port,  SLOT_SE_IO_Pin );
	HAL_GPIO_DeInit(SLOT_SE_RST_GPIO_Port, SLOT_SE_RST_Pin);
	HAL_GPIO_DeInit(SLOT_SE_VDD_GPIO_Port, SLOT_SE_VDD_Pin);

	/* DeInit USART */
	HAL_NVIC_DisableIRQ(SLOT_SE_IRQn);

	if(HAL_SMARTCARD_DeInit(&hsmartcard) != HAL_OK){ return sc_Status_DeInit_Error; }

	SLOT_SE_USART_CLK_DISABLE();

	/* Delete semaphore */
	vSemaphoreDelete(sc_txrx_sem);

	return sc_Status_Success;
}

sc_Status	SE_activate				( sc_class_t class ){

	if (class != class_C) {
		return sc_Status_Unsuported_feature;
	}
	/* To perform a warm reset */
	enable_rst(true);

	/* Power on SE */
	enable_vcc(true);

	/* Enable clk and I/O */
	SET_BIT(hsmartcard.Instance->CR1, USART_CR1_UE);
	SET_BIT(hsmartcard.Instance->CR2, USART_CR2_CLKEN);

	/* wait > 400/f */
	vTaskDelay(2);

	/* Release rst */
	enable_rst(false);

	return sc_Status_Success;
}

sc_Status	SE_deactivate			( void ){

	/* Set reset L */
	enable_rst(true);

	/* Stop clock & I/O */
	CLEAR_BIT(hsmartcard.Instance->CR1, USART_CR1_UE);
	CLEAR_BIT(hsmartcard.Instance->CR2, USART_CR2_CLKEN);

	/* Power off SE */
	enable_vcc(false);

	return sc_Status_Success;
}

sc_Status	SE_get_state			(bool* present, bool* powered){
	// SE is alway present
	*present = true;

	*powered = (bool)HAL_GPIO_ReadPin(SLOT_SE_VDD_GPIO_Port, SLOT_SE_VDD_Pin);
	
	return sc_Status_Success;
}

sc_Status	SE_send_byte			( uint8_t byte ){
	return SE_send_bytes(&byte, 1);
}

#ifdef USE_ACTIVE_WAIT
sc_Status	SE_send_bytes			( uint8_t* ptr, uint32_t len ){
	if (HAL_SMARTCARD_Transmit(&hsmartcard, ptr, len, timeout_ms) != HAL_OK){
		return sc_Status_Hardware_Error;
	}

	return sc_Status_Success;
}
#else
sc_Status	SE_send_bytes			( uint8_t* ptr, uint32_t len ){
	if (HAL_SMARTCARD_Transmit_IT(&hsmartcard, ptr, len) != HAL_OK){
		return sc_Status_Hardware_Error;
	}

	/* Wait Interrupt context to release sem */
	if (xSemaphoreTake(sc_txrx_sem, portMAX_DELAY) != pdPASS){
		return sc_status_os_error;
	}

	return sc_Status_Success;
}
#endif

sc_Status	SE_receive_byte			( uint8_t* byte ){
	return SE_receive_bytes(byte, 1);
}

#ifdef USE_ACTIVE_WAIT
sc_Status	SE_receive_bytes		( uint8_t* ptr, uint32_t len ){
	sc_Status ret;

	if ( HAL_SMARTCARD_Receive(&hsmartcard, ptr, len, timeout_ms) != HAL_OK){
		return sc_Status_Hardware_Error;
	}

	return sc_Status_Success;
}
#else
sc_Status	SE_receive_bytes		( uint8_t* ptr, uint32_t len ){
	sc_Status ret;

	if ( HAL_SMARTCARD_Receive_IT(&hsmartcard, ptr, len) != HAL_OK){
		return sc_Status_Hardware_Error;
	}

	SE_timeout_start();

	/* Wait Interrupt context to release sem */
	if (xSemaphoreTake(sc_txrx_sem, portMAX_DELAY) != pdPASS){
		return sc_status_os_error;
	}

	switch (hsmartcard.ErrorCode) {
	  case HAL_SMARTCARD_ERROR_NONE :
		  ret = sc_Status_Success;
		  break;
	  case HAL_SMARTCARD_ERROR_PE   :
		  ret = sc_Status_Slot_Parity_Error;
		  break;
	  case HAL_SMARTCARD_ERROR_FE   :
		  ret = sc_Status_Slot_Framing_Error;
		  break;
	  case HAL_SMARTCARD_ERROR_ORE  :
		  ret = sc_Status_Slot_Busy_Ressource;
		  break;
	  case HAL_SMARTCARD_ERROR_RTO  :
		  ret = sc_Status_Slot_Reception_Timeout;
		  break;
	  default :
		  ret = sc_Status_Hardware_Error;
		  break;
	}

	SE_timeout_stop();

	return ret;
}
#endif

sc_Status	SE_set_frequency		( uint32_t frequency ){
	sc_Status ret;
	uint32_t prescaler = 0; /* can go from 0x01 to 0x1F*/
	uint32_t pclk, nearest_frequency = frequency + 1;

	pclk = SLOT_SE_GetCLK();

	while((nearest_frequency > frequency) && (++prescaler <= 0x01F)){
		nearest_frequency = pclk / (2 * prescaler);
	}

	if(prescaler > 0x1F){
		return sc_Status_Unsuported_feature;
	}

	current_f = pclk / (2*prescaler);
	hsmartcard.Init.Prescaler = prescaler;

	if ((ret = SE_set_F_D(current_F, current_D)) != sc_Status_Success){
		//@TODO restore frequency
		return ret;
	}
	return sc_Status_Success;
}

sc_Status	SE_get_frequency		( uint32_t* frequency ){
	if(!frequency){
		return sc_Status_Invalid_Parameter;
	}

	*frequency = current_f;
	return sc_Status_Success;
}

sc_Status	SE_set_timeout_etu		( uint32_t timeout ){

	/* check if nack bit is enabled */
	if (READ_BIT(hsmartcard.Instance->CR3, USART_CR3_NACK)){
		if (timeout < MIN_WT || timeout > MAX_WT){
			return sc_Status_Invalid_Parameter;
		}
		MODIFY_REG(hsmartcard.Instance->RTOR, USART_RTOR_RTO, timeout - MIN_WT);

	}else{
		if (timeout < MIN_CWT || timeout > MAX_CWT ){
			return sc_Status_Invalid_Parameter;
		}
		MODIFY_REG(hsmartcard.Instance->RTOR, USART_RTOR_RTO, timeout - MIN_CWT);
	}

	timeout_etu = timeout;
	timeout_ms = (timeout*current_F)/(current_D * (current_f/1000));

	return SE_timeout_set_WT_ms(timeout_ms);
}

sc_Status	SE_get_timeout_etu		( uint32_t* timeout ){
	if(!timeout){
		return sc_Status_Invalid_Parameter;
	}

	*timeout = timeout_etu;
	return  sc_Status_Success;
}

sc_Status	SE_set_guardtime_etu	( uint8_t guardtime ){
	if(guardtime < MIN_GT || guardtime > MAX_GT) {
		return sc_Status_Invalid_Parameter;
	}

	guardtime_etu = guardtime;

	MODIFY_REG(hsmartcard.Instance->GTPR, USART_GTPR_GT, ((uint8_t)guardtime - 11) << USART_GTPR_GT_Pos);

	return sc_Status_Success;
}

sc_Status	SE_get_guardtime_etu	( uint8_t* guardtime ){

	if(!guardtime){
		return sc_Status_Invalid_Parameter;
	}

	*guardtime = guardtime_etu;
	return sc_Status_Success;
}

sc_Status	SE_set_convention		( sc_convention_t convention ){
	if (convention == convention_direct) {
		CLEAR_BIT(hsmartcard.Instance->CR2, USART_CR2_MSBFIRST);
		CLEAR_BIT(hsmartcard.Instance->CR2, USART_CR2_DATAINV);

		return sc_Status_Success;
	}

	if (convention == convention_reverse){
		SET_BIT(hsmartcard.Instance->CR2, USART_CR2_MSBFIRST);
		SET_BIT(hsmartcard.Instance->CR2, USART_CR2_DATAINV);

		return sc_Status_Success;
	}

	return sc_Status_Invalid_Parameter;
}

sc_Status	SE_get_convention		( sc_convention_t* convention ){

	if(!convention){
		return sc_Status_Invalid_Parameter;
	}

	if ( READ_BIT(hsmartcard.Instance->CR2, USART_CR2_DATAINV) ){
		*convention = convention_direct;
	}else {
		*convention = convention_reverse;
	}
	return sc_Status_Success;
}

sc_Status	SE_set_F_D				( uint32_t F, uint32_t D){

	if(F == 0 || D == 0){
		return sc_Status_Invalid_Parameter;
	}

	/* Usart1 is on PCLK2 */
	hsmartcard.Init.BaudRate = compute_baudrate(F, D);

	current_F = F;
	current_D = D;

	if(HAL_SMARTCARD_Init(&hsmartcard) != HAL_OK){
		return sc_Status_Init_Error;
	}

	return sc_Status_Success;
}

sc_Status	SE_get_F_D				( uint32_t* F, uint32_t* D){

	if (!F || !D){
		return sc_Status_Invalid_Parameter;
	}

	*F = current_F;
	*D = current_D;

	return sc_Status_Success;
}

sc_Status	SE_set_IFSD				( uint8_t IFSD ){
	current_IFSD = IFSD;
	return sc_Status_Success;

}

sc_Status	SE_get_IFSD				( uint8_t* IFSD ){
	if( !IFSD ){
		return sc_Status_Invalid_Parameter;
	}
	*IFSD = current_IFSD;
	return sc_Status_Success;

}


/* callbacks */

void					SE_USART_IRQHandler			(void)
{
	/* Handle interrupt */
	HAL_SMARTCARD_IRQHandler(&hsmartcard);

	/* Rearm timeout counter */
	SE_timeout_rearm();
}

#ifndef USE_ACTIVE_WAIT
void HAL_SMARTCARD_ErrorCallback(SMARTCARD_HandleTypeDef *hsmartcard){
	UNUSED(hsmartcard);
	//xSemaphoreGiveFromISR(sc_txrx_sem, NULL);
}

void HAL_SMARTCARD_TxCpltCallback(SMARTCARD_HandleTypeDef *hsmartcard){
	portBASE_TYPE taskWoken = pdFALSE;
	UNUSED(hsmartcard);

	if (xSemaphoreGiveFromISR(sc_txrx_sem, &taskWoken) != pdTRUE) {
	  Error_Handler();
	}
	portEND_SWITCHING_ISR(taskWoken);
}

void HAL_SMARTCARD_RxCpltCallback(SMARTCARD_HandleTypeDef *hsmartcard){
	portBASE_TYPE taskWoken = pdFALSE;
	UNUSED(hsmartcard);

	if (xSemaphoreGiveFromISR(sc_txrx_sem, &taskWoken) != pdTRUE) {
	  Error_Handler();
	}
	portEND_SWITCHING_ISR(taskWoken);
}

void HAL_SMARTCARD_AbortReceiveCpltCallback(SMARTCARD_HandleTypeDef *hsmartcard){
	portBASE_TYPE taskWoken = pdFALSE;
	UNUSED(hsmartcard);

	if (xSemaphoreGiveFromISR(sc_txrx_sem, &taskWoken) != pdTRUE) {
	  Error_Handler();
	}
	portEND_SWITCHING_ISR(taskWoken);
}
#endif
