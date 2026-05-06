/*
 * platform_config.h
 *
 *  Created on: Mar 28, 2018
 *      Author: root
 *
 *      Ressources configuration for stm32l443 64pins
 */

#include "stm32l443xx.h"

#ifndef PLATFORM_CONFIG_H_
#define PLATFORM_CONFIG_H_

/* GPIO
 * == GPIO A ==
 * -PA0  :
 * -PA1  :
 * -PA2  : USART_TX_Pin
 * -PA3  : USART_RX_Pin
 * -PA4  :
 * -PA5  : LD2_Pin
 * -PA6  :
 * -PA7  :
 * -PA8  : SLOT_SE_CK_Pin
 * -PA9  : SLOT_SE_IO_Pin
 * -PA10 : SLOT_SE_RST_Pin
 * -PA11 :
 * -PA12 :
 * -PA13 : TMS_Pin
 * -PA14 : TCK_Pin
 * -PA15 :
 *
 * == GPIO B ==
 * -PB0  :
 * -PB1  :
 * -PB2  :
 * -PB3  : SWO_Pin
 * -PB4  :
 * -PB5  :
 * -PB6  :
 * -PB7  :
 * -PB8  :
 * -PB9  :
 * -PB10 :
 * -PB11 :
 * -PB12 :
 * -PB13 :
 * -PB14 :
 * -PB15 :
 *
 * == GPIO C ==
 * -PC0  :
 * -PC1  :
 * -PC2  :
 * -PC3  :
 * -PC4  :
 * -PC5  :
 * -PC6  :
 * -PC7  :
 * -PC8  :
 * -PC9  : SLOT_SE_VDD_Pin
 * -PC10 :
 * -PC11 :
 * -PC12 :
 * -PC13 : B1_Pin
 * -PC14 :
 * -PC15 :
 *
 * == GPIO C ==
 * -PD2  :
 *
 * == GPIO H ==
 * -PH0  : (OSC_IN)
 * -PH1  : (OSC_OUT)
 * -PH3  : (BOOT0)
 *
 */
#define B1_Pin						GPIO_PIN_13
#define B1_GPIO_Port				GPIOC

#define USART_TX_Pin				GPIO_PIN_2
#define USART_TX_GPIO_Port			GPIOA
#define USART_RX_Pin				GPIO_PIN_3
#define USART_RX_GPIO_Port			GPIOA

#define LD2_Pin						GPIO_PIN_5
#define LD2_GPIO_Port				GPIOA

#define SLOT_SE_GPIO_AF				GPIO_AF7_USART1
#define SLOT_SE_RST_Pin				GPIO_PIN_10
#define SLOT_SE_RST_GPIO_Port		GPIOA
#define SLOT_SE_VDD_Pin				GPIO_PIN_9
#define SLOT_SE_VDD_GPIO_Port		GPIOC
#define SLOT_SE_CK_Pin				GPIO_PIN_8
#define SLOT_SE_CK_GPIO_Port		GPIOA
#define SLOT_SE_IO_Pin				GPIO_PIN_9
#define SLOT_SE_IO_GPIO_Port		GPIOA
#define SLOT_SE_PORT_CLK_ENABLE()	__HAL_RCC_GPIOC_CLK_ENABLE();__HAL_RCC_GPIOA_CLK_ENABLE()
#define SLOT_SE_PORT_CLK_DISABLE()	__HAL_RCC_GPIOC_CLK_DISABLE();__HAL_RCC_GPIOA_CLK_DISABLE()

#define TMS_Pin						GPIO_PIN_13
#define TMS_GPIO_Port				GPIOA
#define TCK_Pin						GPIO_PIN_14
#define TCK_GPIO_Port				GPIOA
#define SWO_Pin						GPIO_PIN_3
#define SWO_GPIO_Port				GPIOB

/* Timers and watchdogs
 * == TIM ==
 * -TIM1  : HAL_TIMEBASE_TIM
 * -TIM2  :
 * -TIM6  : SLOT_SE_TIMEOUT_TIM
 * -TIM7  :
 * -TIM15 :
 * -TIM16 :
 *
 * == LPTIM ==
 * -LPTIM1 :
 * -LPTIM2 :
 *
 * == watchdogs ==
 * -IWDG :
 * -WWDG :
 *
 * == SysTick ==
 * -SysTick : FreeRTOS
 */

#define HAL_TIMEBASE_TIM			TIM1
#define SLOT_SE_TIMEOUT_TIM			TIM6
#define SLOT_SE_TIM_CLK_ENABLE()	__HAL_RCC_TIM6_CLK_ENABLE();
#define SLOT_SE_TIM_CLK_DISABLE()	__HAL_RCC_TIM6_CLK_DISABLE();

/* Others peripherals
 * == MPU ==
 * -MPU  :
 *
 * == Firewall ==
 *
 * == CRC ==
 * -CRC  :
 *
 * == DMA ==
 * -DMA1 :
 * -DMA2 :
 *
 * == ADC ==
 * -ADC1 :
 *
 * == DAC ==
 * -DAC1 :
 *
 * == VREFBUF ==
 * -VREFBUF :
 *
 * == COMP =
 * -COMP1 :
 * -COMP2 :
 *
 * == OPAMP ==
 * -OPAMP :
 *
 * == TSC ==
 * -TSC  :
 *
 * == LCD ==
 * -LCD  :
 *
 * == RNG ==
 * -RNG  :
 *
 * == I2C ==
 * -I2C1 :
 * -I2C2 :
 * -I2C3 :
 *
 * == USART ==
 * -USART1 : SLOT_SE_USART
 * -USART2 :
 * -USART3 :
 *
 * == LPUART ==
 * -LPUART1 :
 *
 * == SPI ==
 * -SPI1 :
 * -SPI2 :
 * -SPI3 :
 *
 * == SAI ==
 * -SAI1 :
 *
 * == SWPMI ==
 * -SWPMI1 :
 *
 * == CAN ==
 * -CAN1 :
 *
 * == SDMMC ==
 * -SDMMC1 :
 *
 * == USB ==
 * -USB  :
 *
 * == CRS ==
 * -CRS  :
 *
 * == QUADSPI ==
 * -QUADSPI :
 *
 */
#define SLOT_SE_USART				USART1
#define SLOT_SE_GetCLK()			HAL_RCC_GetPCLK2Freq()
#define SLOT_SE_USART_CLK_ENABLE()	__HAL_RCC_USART1_CLK_ENABLE()
#define SLOT_SE_USART_CLK_DISABLE()	__HAL_RCC_USART1_CLK_DISABLE()

/* Interrupt Vector and Callbacks
 *
 * NonMaskableInt_IRQn   :
 * HardFault_IRQn        :
 * MemoryManagement_IRQn :
 * BusFault_IRQn         :
 * UsageFault_IRQn       :
 * SVCall_IRQn           :
 * DebugMonitor_IRQn     :
 * PendSV_IRQn           :
 * SysTick_IRQn          :
 *******  STM32 specific :
 * WWDG_IRQn             :
 * PVD_PVM_IRQn          :
 * TAMP_STAMP_IRQn       :
 * RTC_WKUP_IRQn         :
 * FLASH_IRQn            :
 * RCC_IRQn              :
 * EXTI0_IRQn            :
 * EXTI1_IRQn            :
 * EXTI2_IRQn            :
 * EXTI3_IRQn            :
 * EXTI4_IRQn            :
 * DMA1_Channel1_IRQn    :
 * DMA1_Channel2_IRQn    :
 * DMA1_Channel3_IRQn    :
 * DMA1_Channel4_IRQn    :
 * DMA1_Channel5_IRQn    :
 * DMA1_Channel6_IRQn    :
 * DMA1_Channel7_IRQn    :
 * ADC1_IRQn             :
 * CAN1_TX_IRQn          :
 * CAN1_RX0_IRQn         :
 * CAN1_RX1_IRQn         :
 * CAN1_SCE_IRQn         :
 * EXTI9_5_IRQn          :
 * TIM1_BRK_TIM15_IRQn   :
 * TIM1_UP_TIM16_IRQn    : HAL_TIMEBASE_IRQn,
 * TIM1_TRG_COM_IRQn     :
 * TIM1_CC_IRQn          :
 * TIM2_IRQn             :
 * I2C1_EV_IRQn          :
 * I2C1_ER_IRQn          :
 * I2C2_EV_IRQn          :
 * I2C2_ER_IRQn          :
 * SPI1_IRQn             :
 * SPI2_IRQn             :
 * USART1_IRQn           : SLOT_SE_IRQn
 * USART2_IRQn           :
 * USART3_IRQn           :
 * EXTI15_10_IRQn        :
 * RTC_Alarm_IRQn        :
 * SDMMC1_IRQn           :
 * SPI3_IRQn             :
 * TIM6_DAC_IRQn         : SLOT_SE_TIMEOUT_IRQn
 * TIM7_IRQn             :
 * DMA2_Channel1_IRQn    :
 * DMA2_Channel2_IRQn    :
 * DMA2_Channel3_IRQn    :
 * DMA2_Channel4_IRQn    :
 * DMA2_Channel5_IRQn    :
 * COMP_IRQn             :
 * LPTIM1_IRQn           :
 * LPTIM2_IRQn           :
 * USB_IRQn              :
 * DMA2_Channel6_IRQn    :
 * DMA2_Channel7_IRQn    :
 * LPUART1_IRQn          :
 * QUADSPI_IRQn          :
 * I2C3_EV_IRQn          :
 * I2C3_ER_IRQn          :
 * SAI1_IRQn             :
 * SWPMI1_IRQn           :
 * TSC_IRQn              :
 * LCD_IRQn              :
 * AES_IRQn              :
 * RNG_IRQn              :
 * FPU_IRQn              :
 * CRS_IRQn              :
 */


/* Interrupt IRQ_handler */

#define HAL_TIMEBASE_IRQn			TIM1_UP_TIM16_IRQn

#define SLOT_SE_IRQn				USART1_IRQn

#define SLOT_SE_TIMEOUT_IRQn		TIM6_DAC_IRQn

#endif /* PLATFORM_CONFIG_H_ */
