/*
 * WBSLOT
 * WBSLOT Driver implementation of interface slotItf
 */

#include "WBSLOT.h"

#include "cmsis_os2.h"
#include "stm32wbxx_hal.h"

#include "sc_defs.h"
#include "slot_itf.h"

/************************************************************************************
 * Private defines
 ************************************************************************************/

#define MIN_GT  11
#define MAX_GT  0xFF
#define MIN_WT  12
#define MAX_WT  0xFFFFFF
#define MIN_CWT 11
#define MAX_CWT 0xFFFFFF
#define MIN_BWT 11
#define MAX_BWT 0xFF

// time between VCC_en and clk_en
#define TIMING_Ta_ms 1
// time between clk_en and rst assert
#define TIMING_Tb_ms 2

#define SEND_TIMEOUT_MS 500

// Uncomment/change following line to negociate speeds with card
// #define MIN_ETU_TIMINGS_NS  2000
#define MIN_ETU_TIMINGS_NS                                                     \
  (4000 + ((LOG_LEVEL_CONF >= logLevel_info) ? 2000 : 0))

/* Debug output control */
#define LOG_LEVEL_CONF userDataApp.logFlags.stpay

/************************************************************************************
 * Private variables
 ************************************************************************************/
static SMARTCARD_HandleTypeDef hsmartcard_p;

#ifdef WBSLOT_USE_CMSISOS
#define WBSLOT_EVENT_FLAG_CPLT_TXRX  0x0001
#define WBSLOT_EVENT_FLAG_CPLT_ABORT 0x0002
#define WBSLOT_EVENT_FLAG_ERROR      0x0004
#define WBSLOT_EVENT_FLAGS_ALL                                                 \
  (WBSLOT_EVENT_FLAG_CPLT_TXRX | WBSLOT_EVENT_FLAG_CPLT_ABORT |                \
   WBSLOT_EVENT_FLAG_ERROR)
static osThreadId_t thread_id;
#else
enum dialstate_e {
  ongoing,
  finished,
  aborted
} dialstate;
#endif

#ifdef WBSLOT_USE_DMA
static DMA_HandleTypeDef hdma_smartcard_rx_p;
static DMA_HandleTypeDef hdma_smartcard_tx_p;
#endif

// ISO7816 parameters
static uint32_t guardtime_etu;
static uint32_t timeout_etu;
static uint32_t timeout_ms;
static uint32_t current_F;
static uint32_t current_D;
static uint32_t current_f;
static uint8_t  current_IFSD;
static bool     noByteExchangedSinceInit;

/************************************************************************************
 * Private types
 ************************************************************************************/

/************************************************************************************
 * Private functions prototypes
 ************************************************************************************/
/* t_slot private functions prototypes */
static sc_Status WBSLOT_init(void);

static sc_Status WBSLOT_deinit(void);

static sc_Status WBSLOT_get_state(bool *present, bool *powered);

static sc_Status WBSLOT_activate(sc_class_t class);

static sc_Status WBSLOT_deactivate(void);

static sc_Status WBSLOT_send_byte(uint8_t byte);

static sc_Status WBSLOT_send_bytes(const uint8_t *ptr, uint32_t len);

static sc_Status WBSLOT_receive_byte(uint8_t *byte);

static sc_Status WBSLOT_receive_bytes(uint8_t *ptr, uint32_t len);

static sc_Status WBSLOT_set_frequency(uint32_t frequency);

static sc_Status WBSLOT_get_frequency(uint32_t *frequency);

static sc_Status WBSLOT_set_timeout_etu(uint32_t timeout);

static sc_Status WBSLOT_get_timeout_etu(uint32_t *timeout);

static sc_Status WBSLOT_set_guardtime_etu(uint32_t guardtime);

static sc_Status WBSLOT_get_guardtime_etu(uint32_t *guardtime);

static sc_Status WBSLOT_set_convention(sc_convention_t convention);

static sc_Status WBSLOT_get_convention(sc_convention_t *convention);

static sc_Status WBSLOT_set_F_D(uint32_t F, uint32_t D);

static sc_Status WBSLOT_get_F_D(uint32_t *F, uint32_t *D);

static sc_Status WBSLOT_set_IFSD(uint8_t IFSD);

static sc_Status WBSLOT_get_IFSD(uint8_t *IFSD);

static sc_Status WBSLOT_get_min_etu_ns(uint32_t *etu_ns);

/************************************************************************************
 * Public variables
 ************************************************************************************/
slot_itf_t hslot_WBSLOT = {WBSLOT_init,
                           WBSLOT_deinit,
                           WBSLOT_get_state,
                           WBSLOT_activate,
                           WBSLOT_deactivate,
                           WBSLOT_send_byte,
                           WBSLOT_send_bytes,
                           WBSLOT_receive_byte,
                           WBSLOT_receive_bytes,
                           WBSLOT_set_frequency,
                           WBSLOT_get_frequency,
                           WBSLOT_set_timeout_etu,
                           WBSLOT_get_timeout_etu,
                           WBSLOT_set_guardtime_etu,
                           WBSLOT_get_guardtime_etu,
                           WBSLOT_set_convention,
                           WBSLOT_get_convention,
                           WBSLOT_set_F_D,
                           WBSLOT_get_F_D,
                           WBSLOT_set_IFSD,
                           WBSLOT_get_IFSD,
                           WBSLOT_get_min_etu_ns};

/************************************************************************************
 * Private functions
 ************************************************************************************/
static void enable_vcc(bool enable) {
  HAL_GPIO_WritePin(WBSLOT_VDD_GPIO_Port, WBSLOT_VDD_Pin, enable);
}

static void enable_rst(bool enable) {

  HAL_GPIO_WritePin(WBSLOT_RST_GPIO_Port, WBSLOT_RST_Pin,
                    enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void enable_clk(bool enable) {
  GPIO_InitTypeDef GPIO_InitStruct;

  GPIO_InitStruct.Pin       = WBSLOT_CK_Pin;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = WBSLOT_GPIO_AF;
  if (enable) {
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    HAL_GPIO_Init(WBSLOT_CK_GPIO_Port, &GPIO_InitStruct);
    SET_BIT(hsmartcard_p.Instance->CR2, USART_CR2_CLKEN);
  } else {
    CLEAR_BIT(hsmartcard_p.Instance->CR2, USART_CR2_CLKEN);

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(WBSLOT_CK_GPIO_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(WBSLOT_CK_GPIO_Port, WBSLOT_CK_Pin, GPIO_PIN_RESET);
  }
}

static uint32_t compute_baudrate(uint32_t F, uint32_t D) {
  /* Baudrate = ( CLK / ( 2*Prescaler )) * (1 / ( F/D )) */
  return ((WBSLOT_GetCLK() / 2) * D) / (hsmartcard_p.Init.Prescaler * F);
}

static sc_Status WBSLOT_init(void) {
#ifdef WBSLOT_USE_CMSISOS
  /* Init thread id */
  thread_id = osThreadGetId();
#endif

  timeout_ms    = SC_DEFAULT_WT_MS;
  timeout_etu   = SC_DEFAULT_WT_ETU;
  guardtime_etu = 11;
  current_F     = SC_Fd;
  current_D     = SC_Dd;
  current_f     = SC_fmaxd;
  current_IFSD  = T1_MAX_DATA_SIZE;

  GPIO_InitTypeDef GPIO_InitStruct;
  /* Peripheral clock enable */

  WBSLOT_USART_CLK_ENABLE();
  WBSLOT_PORT_CLK_ENABLE();

#ifdef WBSLOT_USE_DMA
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  WBSLOT_DMA_CLK_ENABLE();
#endif

  /*Configure GPIO pin : ISO7816_VDD_Pin */
  GPIO_InitStruct.Pin   = WBSLOT_VDD_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(WBSLOT_VDD_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ISO7816_RST_Pin */
  GPIO_InitStruct.Pin   = WBSLOT_RST_Pin;
  GPIO_InitStruct.Pull  = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
  HAL_GPIO_Init(WBSLOT_RST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ISO7816_IO_Pin */
  GPIO_InitStruct.Pin       = WBSLOT_IO_Pin;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Alternate = WBSLOT_GPIO_AF;
  HAL_GPIO_Init(WBSLOT_IO_GPIO_Port, &GPIO_InitStruct);

#ifdef WBSLOT_USE_DMA
  /* DMA_TX Init */
  hdma_smartcard_tx_p.Instance                 = WBSLOT_TX_DMA_CH;
  hdma_smartcard_tx_p.Init.Request             = WBSLOT_TX_DMA_REQUEST;
  hdma_smartcard_tx_p.Init.Direction           = DMA_MEMORY_TO_PERIPH;
  hdma_smartcard_tx_p.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_smartcard_tx_p.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_smartcard_tx_p.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_smartcard_tx_p.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
  hdma_smartcard_tx_p.Init.Mode                = DMA_NORMAL;
  hdma_smartcard_tx_p.Init.Priority            = DMA_PRIORITY_MEDIUM;
  if (HAL_DMA_Init(&hdma_smartcard_tx_p) != HAL_OK) {
    return sc_Status_Init_Error;
  }
  __HAL_LINKDMA(&hsmartcard_p, hdmatx, hdma_smartcard_tx_p);

  /* DMA_RX Init */
  hdma_smartcard_rx_p.Instance                 = WBSLOT_RX_DMA_CH;
  hdma_smartcard_rx_p.Init.Request             = WBSLOT_RX_DMA_REQUEST;
  hdma_smartcard_rx_p.Init.Direction           = DMA_PERIPH_TO_MEMORY;
  hdma_smartcard_rx_p.Init.PeriphInc           = DMA_PINC_DISABLE;
  hdma_smartcard_rx_p.Init.MemInc              = DMA_MINC_ENABLE;
  hdma_smartcard_rx_p.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_smartcard_rx_p.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
  hdma_smartcard_rx_p.Init.Mode                = DMA_NORMAL;
  hdma_smartcard_rx_p.Init.Priority            = DMA_PRIORITY_MEDIUM;
  if (HAL_DMA_Init(&hdma_smartcard_rx_p) != HAL_OK) {
    return sc_Status_Init_Error;
  }
  __HAL_LINKDMA(&hsmartcard_p, hdmarx, hdma_smartcard_rx_p);
#endif

  hsmartcard_p.Instance            = WBSLOT_USART;
  hsmartcard_p.Init.Mode           = SMARTCARD_MODE_TX_RX;
  hsmartcard_p.Init.Prescaler      = (WBSLOT_GetCLK() / current_f) / 2;
  hsmartcard_p.Init.BaudRate       = compute_baudrate(current_F, current_D);
  hsmartcard_p.Init.WordLength     = SMARTCARD_WORDLENGTH_9B;
  hsmartcard_p.Init.StopBits       = SMARTCARD_STOPBITS_1_5;
  hsmartcard_p.Init.Parity         = SMARTCARD_PARITY_EVEN;
  hsmartcard_p.Init.OneBitSampling = SMARTCARD_ONE_BIT_SAMPLE_ENABLE;
  hsmartcard_p.Init.NACKEnable     = SMARTCARD_NACK_DISABLE;
  hsmartcard_p.Init.GuardTime      = 0;
  hsmartcard_p.Init.TimeOutEnable  = SMARTCARD_TIMEOUT_DISABLE;
  hsmartcard_p.Init.TimeOutValue   = timeout_etu;
  hsmartcard_p.Init.BlockLength    = 0;
  hsmartcard_p.Init.AutoRetryCount = 0;
  hsmartcard_p.AdvancedInit.AdvFeatureInit = SMARTCARD_ADVFEATURE_NO_INIT;
  if (HAL_SMARTCARD_Init(&hsmartcard_p) != HAL_OK) {
    return sc_Status_Init_Error;
  }

  // Enable IRQ and set them under os irq and timeout irq
  HAL_NVIC_EnableIRQ(WBSLOT_IRQn);
  HAL_NVIC_SetPriority(WBSLOT_IRQn, 5, 0);
#ifdef WBSLOT_USE_DMA
  /* WBSLOT_TX_DMA_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(WBSLOT_TX_DMA_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(WBSLOT_TX_DMA_IRQn);
  /* WBSLOT_RX_DMA_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(WBSLOT_RX_DMA_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(WBSLOT_RX_DMA_IRQn);
#endif

  WBSLOT_deactivate();

  return sc_Status_Success;
}

static sc_Status WBSLOT_deinit(void) {
  /* DeInit IOs */
  HAL_GPIO_DeInit(WBSLOT_CK_GPIO_Port, WBSLOT_CK_Pin);
  HAL_GPIO_DeInit(WBSLOT_IO_GPIO_Port, WBSLOT_IO_Pin);
  HAL_GPIO_DeInit(WBSLOT_RST_GPIO_Port, WBSLOT_RST_Pin);
  HAL_GPIO_DeInit(WBSLOT_VDD_GPIO_Port, WBSLOT_VDD_Pin);

  /* DeInit USART */
  HAL_NVIC_DisableIRQ(WBSLOT_IRQn);
#ifdef WBSLOT_USE_DMA
  HAL_NVIC_DisableIRQ(WBSLOT_TX_DMA_IRQn);
  HAL_NVIC_DisableIRQ(WBSLOT_RX_DMA_IRQn);
#endif

  if (HAL_SMARTCARD_DeInit(&hsmartcard_p) != HAL_OK) {
    return sc_Status_DeInit_Error;
  }

  WBSLOT_USART_CLK_DISABLE();

  return sc_Status_Success;
}

static sc_Status WBSLOT_activate(sc_class_t class) {
  noByteExchangedSinceInit = true;

  if (class != class_C) {
    return sc_Status_Unsuported_feature;
  }
  /* To perform a warm reset */
  enable_rst(true);

  /* Power on SE */
  enable_vcc(true);

  enable_clk(true);

  /* Give time to power to stabilize */

#ifdef WBSLOT_USE_CMSISOS
  osDelay(TIMING_Ta_ms);
#else
  HAL_Delay(TIMING_Ta_ms);
#endif

  /* Enable clk and I/O */
  SET_BIT(hsmartcard_p.Instance->CR1, USART_CR1_UE);

#ifdef WBSLOT_USE_CMSISOS
  /* wait > 400/f */
  osDelay(TIMING_Tb_ms - TIMING_Ta_ms);
#else
  HAL_Delay(TIMING_Tb_ms - TIMING_Ta_ms);
#endif

  SET_BIT(hsmartcard_p.Instance->RQR, SMARTCARD_RXDATA_FLUSH_REQUEST);
#if 0
    /* Patch for unresolved USART first read fail */
        uint8_t dummy;
        uint32_t old_timeout_ms = timeout_ms;
        timeout_ms = 0;
        WBSLOT_receive_byte(&dummy);
        timeout_ms = old_timeout_ms;
#endif

  /* Release rst */
  enable_rst(false);

  return sc_Status_Success;
}

static sc_Status WBSLOT_deactivate(void) {
  /* Set reset L */
  enable_rst(true);

  /* Stop clock & I/O */
  CLEAR_BIT(hsmartcard_p.Instance->CR1, USART_CR1_UE);

  /* Power off SE */
  enable_vcc(false);

  enable_clk(false);

  return sc_Status_Success;
}

static sc_Status WBSLOT_get_state(bool *present, bool *powered) {
  // SE is alway present
  *present = true;

  *powered = (bool)HAL_GPIO_ReadPin(WBSLOT_VDD_GPIO_Port, WBSLOT_VDD_Pin);

  return sc_Status_Success;
}

static sc_Status WBSLOT_send_byte(uint8_t byte) {
  return WBSLOT_send_bytes(&byte, 1);
}

static sc_Status WBSLOT_send_bytes(const uint8_t *ptr, uint32_t len) {
#ifdef WBSLOT_USE_CMSISOS
  int32_t flags_ret = 0;

  // Remove any pending token
  osThreadFlagsClear(WBSLOT_EVENT_FLAGS_ALL);
#else
  dialstate = ongoing;
#endif

#ifdef WBSLOT_USE_DMA
  if (HAL_SMARTCARD_Transmit_DMA(&hsmartcard_p, ptr, len) != HAL_OK)
#else
  if (HAL_SMARTCARD_Transmit_IT(&hsmartcard_p, ptr, len) != HAL_OK)
#endif
  {
    HAL_SMARTCARD_Abort_IT(&hsmartcard_p);
    return sc_Status_Hardware_Error;
  }

  /* Wait Interrupt context to release sem */
#ifdef WBSLOT_USE_CMSISOS

  flags_ret = (int32_t)osThreadFlagsWait(WBSLOT_EVENT_FLAGS_ALL, osFlagsWaitAny,
                                         SEND_TIMEOUT_MS);

  if (flags_ret < 0) {
    HAL_SMARTCARD_Abort_IT(&hsmartcard_p);
    switch (flags_ret) {
    case osOK:
      break;
    case osErrorTimeout:
      return sc_Status_Hardware_Error;
    default:
      return sc_status_os_error;
    }
  }
  // WBSLOT_EVENT_FLAG_ERROR handled below

#else
  uint32_t tickstart = HAL_GetTick();
  while (dialstate == ongoing) {
    if (((HAL_GetTick() - tickstart) > timeout_ms)) {
      HAL_SMARTCARD_Abort_IT(&hsmartcard_p);
      return sc_Status_Slot_Reception_Timeout;
    }
  }
#endif

  switch (hsmartcard_p.ErrorCode) {
  case HAL_SMARTCARD_ERROR_NONE:
    break;
  case HAL_SMARTCARD_ERROR_PE:
    return sc_Status_Slot_Parity_Error;
  case HAL_SMARTCARD_ERROR_FE:
    return sc_Status_Slot_Framing_Error;
  case HAL_SMARTCARD_ERROR_ORE:
    return sc_Status_Slot_Busy_Ressource;
  case HAL_SMARTCARD_ERROR_RTO:
    return sc_Status_Slot_Reception_Timeout;
  default:
    return sc_Status_Hardware_Error;
  }

  return sc_Status_Success;
}

static sc_Status WBSLOT_receive_byte(uint8_t *byte) {
  sc_Status st           = WBSLOT_receive_bytes(byte, 1);
  uint8_t   retryCounter = 4;
  /* If this is the first receive byte after a power on (ATR retrieval),
   * implement retry */
  while ((st != sc_Status_Success) && (noByteExchangedSinceInit == true) &&
         ((--retryCounter) > 0))
    st = WBSLOT_receive_bytes(byte, 1);

  noByteExchangedSinceInit = false;
  return st;
}

static sc_Status WBSLOT_receive_bytes(uint8_t *ptr, uint32_t len) {
#ifdef WBSLOT_USE_CMSISOS
  int32_t flags_ret = 0;

  // Remove any pending token
  osThreadFlagsClear(WBSLOT_EVENT_FLAGS_ALL);
#else
  dialstate = ongoing;
#endif

#ifdef WBSLOT_USE_DMA
  if (HAL_SMARTCARD_Receive_DMA(&hsmartcard_p, ptr, len) != HAL_OK)
#else
  if (HAL_SMARTCARD_Receive_IT(&hsmartcard_p, ptr, len) != HAL_OK)
#endif
  {
    HAL_SMARTCARD_Abort_IT(&hsmartcard_p);
    return sc_Status_Hardware_Error;
  }

  /* Wait Interrupt context to release sem */
#ifdef WBSLOT_USE_CMSISOS

  flags_ret = (int32_t)osThreadFlagsWait(WBSLOT_EVENT_FLAGS_ALL, osFlagsWaitAny,
                                         timeout_ms);

  if (flags_ret < 0) {
    HAL_SMARTCARD_Abort_IT(&hsmartcard_p);
    switch (flags_ret) {
    case osErrorTimeout:
      return sc_Status_Slot_Reception_Timeout;
    default:
      return sc_status_os_error;
    }
  }
  // WBSLOT_EVENT_FLAG_ERROR handled below

#else

  uint32_t tickstart = HAL_GetTick();
  while (dialstate == ongoing) {
    if (((HAL_GetTick() - tickstart) > timeout_ms)) {
      HAL_SMARTCARD_Abort_IT(&hsmartcard_p);
      return sc_Status_Slot_Reception_Timeout;
    }
  }

#endif

  switch (hsmartcard_p.ErrorCode) {
  case HAL_SMARTCARD_ERROR_NONE:
    break;
  case HAL_SMARTCARD_ERROR_PE:
    return sc_Status_Slot_Parity_Error;
  case HAL_SMARTCARD_ERROR_FE:
    return sc_Status_Slot_Framing_Error;
  case HAL_SMARTCARD_ERROR_ORE:
    return sc_Status_Slot_Busy_Ressource;
  case HAL_SMARTCARD_ERROR_RTO:
    return sc_Status_Slot_Reception_Timeout;
  default:
    return sc_Status_Hardware_Error;
  }

  return sc_Status_Success;
}

static sc_Status WBSLOT_set_frequency(uint32_t frequency) {
  sc_Status ret;
  uint32_t  prescaler = 1; /* can go from 0x01 to 0x1F*/
  uint32_t  pclk, nearest_frequency;

  pclk = WBSLOT_GetCLK();

  do {
    nearest_frequency = pclk / (2 * prescaler);
  } while ((nearest_frequency > frequency) && (++prescaler <= 0x01F));

  if (prescaler > 0x1F) {
    return sc_Status_Unsuported_feature;
  }

  current_f                   = pclk / (2 * prescaler);
  hsmartcard_p.Init.Prescaler = prescaler;

  if ((ret = WBSLOT_set_F_D(current_F, current_D)) != sc_Status_Success) {
    return ret;
  }
  return sc_Status_Success;
}

static sc_Status WBSLOT_get_frequency(uint32_t *frequency) {
  if (!frequency) {
    return sc_Status_Invalid_Parameter;
  }

  *frequency = current_f;
  return sc_Status_Success;
}

static sc_Status WBSLOT_set_timeout_etu(uint32_t timeout) {

  /* check if nack bit is enabled */
  if (READ_BIT(hsmartcard_p.Instance->CR3, USART_CR3_NACK)) {
    if (timeout < MIN_WT || timeout > MAX_WT) {
      return sc_Status_Invalid_Parameter;
    }

  } else {
    if (timeout < MIN_CWT || timeout > MAX_CWT) {
      return sc_Status_Invalid_Parameter;
    }
  }

  timeout_etu = timeout;
  timeout_ms  = (timeout * current_F) / (current_D * (current_f / 1000));

  if (timeout_ms == 0)
    timeout_ms = 1;

  return sc_Status_Success;
}

static sc_Status WBSLOT_get_timeout_etu(uint32_t *timeout) {
  if (!timeout) {
    return sc_Status_Invalid_Parameter;
  }

  *timeout = timeout_etu;
  return sc_Status_Success;
}

static sc_Status WBSLOT_set_guardtime_etu(uint32_t guardtime) {
  if (guardtime < MIN_GT) // || guardtime > MAX_GT)
  {
    return sc_Status_Invalid_Parameter;
  }

  guardtime_etu = guardtime;

  MODIFY_REG(hsmartcard_p.Instance->GTPR, USART_GTPR_GT,
             ((uint8_t)guardtime - 11) << USART_GTPR_GT_Pos);

  return sc_Status_Success;
}

static sc_Status WBSLOT_get_guardtime_etu(uint32_t *guardtime) {
  if (!guardtime) {
    return sc_Status_Invalid_Parameter;
  }

  *guardtime = guardtime_etu;
  return sc_Status_Success;
}

static sc_Status WBSLOT_set_convention(sc_convention_t convention) {
  if (convention == convention_direct) {
    CLEAR_BIT(hsmartcard_p.Instance->CR2, USART_CR2_MSBFIRST);
    CLEAR_BIT(hsmartcard_p.Instance->CR2, USART_CR2_DATAINV);

    return sc_Status_Success;
  }

  if (convention == convention_reverse) {
    SET_BIT(hsmartcard_p.Instance->CR2, USART_CR2_MSBFIRST);
    SET_BIT(hsmartcard_p.Instance->CR2, USART_CR2_DATAINV);

    return sc_Status_Success;
  }

  return sc_Status_Invalid_Parameter;
}

static sc_Status WBSLOT_get_convention(sc_convention_t *convention) {

  if (!convention) {
    return sc_Status_Invalid_Parameter;
  }

  if (READ_BIT(hsmartcard_p.Instance->CR2, USART_CR2_DATAINV)) {
    *convention = convention_direct;
  } else {
    *convention = convention_reverse;
  }
  return sc_Status_Success;
}

static sc_Status WBSLOT_set_F_D(uint32_t F, uint32_t D) {
  if (F == 0 || D == 0) {
    return sc_Status_Invalid_Parameter;
  }

  /* Usart1 is on PCLK2 */
  hsmartcard_p.Init.BaudRate = compute_baudrate(F, D);

  current_F = F;
  current_D = D;

  if (HAL_SMARTCARD_Init(&hsmartcard_p) != HAL_OK) {
    return sc_Status_Init_Error;
  }

  return sc_Status_Success;
}

static sc_Status WBSLOT_get_F_D(uint32_t *F, uint32_t *D) {
  if (!F || !D) {
    return sc_Status_Invalid_Parameter;
  }

  *F = current_F;
  *D = current_D;

  return sc_Status_Success;
}

static sc_Status WBSLOT_set_IFSD(uint8_t IFSD) {
  current_IFSD = IFSD;
  return sc_Status_Success;
}

static sc_Status WBSLOT_get_IFSD(uint8_t *IFSD) {
  if (!IFSD) {
    return sc_Status_Invalid_Parameter;
  }
  *IFSD = current_IFSD;
  return sc_Status_Success;
}

static sc_Status WBSLOT_get_min_etu_ns(uint32_t *etu_ns) {
  if (etu_ns == NULL)
    return sc_Status_Invalid_Parameter;

#ifdef MIN_ETU_TIMINGS_NS
  *etu_ns = MIN_ETU_TIMINGS_NS;
  return sc_Status_Success;
#else
  *etu_ns = 0;
  return sc_Status_Unsuported_feature;
#endif
}

/************************************************************************************
 * Public functions
 ************************************************************************************/
void WBSLOT_USART_IRQHandler(void) {
  /* Handle interrupt, call SMARTCARD callbacks bellow */
  HAL_SMARTCARD_IRQHandler(&hsmartcard_p);
}

#ifdef WBSLOT_USE_DMA
void WBSLOT_DMA_TX_IRQHandler(void) {
  /* Handle interrupt, call DMA callbacks bellow */
  HAL_DMA_IRQHandler(&hdma_smartcard_tx_p);
}

void WBSLOT_DMA_RX_IRQHandler(void) {
  /* Handle interrupt, call DMA callbacks bellow */
  HAL_DMA_IRQHandler(&hdma_smartcard_rx_p);
}
#endif

/************************************************************************************
 * Overloaded functions
 ************************************************************************************/

void HAL_SMARTCARD_ErrorCallback(SMARTCARD_HandleTypeDef *hsmartcard) {
  UNUSED(hsmartcard);
#ifdef WBSLOT_USE_CMSISOS
  osThreadFlagsSet(thread_id, WBSLOT_EVENT_FLAG_ERROR);
#else
  dialstate = finished;
#endif
}

void HAL_SMARTCARD_TxCpltCallback(SMARTCARD_HandleTypeDef *hsmartcard) {
  UNUSED(hsmartcard);
#ifdef WBSLOT_USE_CMSISOS
  osThreadFlagsSet(thread_id, WBSLOT_EVENT_FLAG_CPLT_TXRX);
#else
  dialstate = finished;
#endif
}

void HAL_SMARTCARD_RxCpltCallback(SMARTCARD_HandleTypeDef *hsmartcard) {
  UNUSED(hsmartcard);
#ifdef WBSLOT_USE_CMSISOS
  osThreadFlagsSet(thread_id, WBSLOT_EVENT_FLAG_CPLT_TXRX);
#else
  dialstate = finished;
#endif
}

void HAL_SMARTCARD_AbortReceiveCpltCallback(
    SMARTCARD_HandleTypeDef *hsmartcard) {
  UNUSED(hsmartcard);
#ifdef WBSLOT_USE_CMSISOS
  osThreadFlagsSet(thread_id, WBSLOT_EVENT_FLAG_CPLT_ABORT);
#else
  dialstate = aborted;
#endif
}
