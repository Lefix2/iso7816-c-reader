#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include "cmsis_os2.h"

#include "stm32l4xx_hal.h"
#include "debug.h"

#ifdef DEBUG

char buffer[255];

UART_HandleTypeDef huart2;

void dbg_Init( void )
{
	  GPIO_InitTypeDef GPIO_InitStruct;

    __HAL_RCC_USART2_CLK_ENABLE();

    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = USART_TX_Pin|USART_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart2) != HAL_OK)
	{
	_Error_Handler(__FILE__, __LINE__);
	}
}

void dbg_Deinit( void )
{
    /* Peripheral clock disable */
    __HAL_RCC_USART2_CLK_DISABLE();

    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    HAL_GPIO_DeInit(GPIOA, USART_TX_Pin|USART_RX_Pin);
}


void dbg_printf(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);

	//osMutexWait(uart_mutex_id, osWaitForever);

	HAL_UART_Transmit(&huart2, (uint8_t*)buffer, strlen(buffer), HAL_UART_TIMEOUT_VALUE);

	//osMutexRelease(uart_mutex_id);
}

#endif /* DEBUG */

void dbg_buff ( const char* info, const char* buffAdd, int len)
{
    const int maxBytesPerLine = 16;

    /* Print the header info, and a line return if the buffer will be multiline */
    dbg_printf("%s:%s", info, (len > maxBytesPerLine ? "\r\n" : " "));

    /* print the buffer by lines of 16 bytes */
    for (int i = 0 ; i < len ; i++)
    {
        dbg_printf(
                "%s"    /* Start of line spacing (only if multiline) or inter char space */
                "%s"    /* possible extra space between blocks of 4 bytes */
                "%02X"
                "%s"    /* possible end of line*/
                ,( (i%maxBytesPerLine == 0) ? (len > maxBytesPerLine ? "    " : "") : " " )
                ,( (i%maxBytesPerLine != 0 && i%4 == 0) ? " " : "" )
                ,buffAdd[i]
                         ,( ((i+1)%maxBytesPerLine == 0) ? "\r\n" : "" )
        );
    }
    /* final end of line, except if already added */
    if (len == 0 || len%maxBytesPerLine != 0)   dbg_printf("\r\n");
}
