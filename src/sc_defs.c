/*
 * Sc defs
 * Definitions for smartcard module according to ISO7816
 */

#include "sc_defs.h"

/************************************************************************************
 * Private defines
 ************************************************************************************/

/************************************************************************************
 * Private variables
 ************************************************************************************/

/* fmax table, indicated fmax for given TA1[8-5] */
static const uint32_t fmax_table[16] = {
    SC_fmaxd, 5000000, 6000000, 8000000,  12000000, 16000000, 20000000, 0,
    0,        5000000, 7500000, 10000000, 15000000, 20000000, 0,        0};

/* Fi table, indicated F for given TA1[8-5] */
static const uint16_t f_table[16] = {SC_Fd, 372,  558, 744, 1116, 1488,
                                     1860,  0,    0,   512, 768,  1024,
                                     1536,  2048, 0,   0};

/* Di table, indicated D for given TA1[4-1] */
static const uint16_t d_table[16] = {0,  SC_Dd, 2, 4, 8, 16, 32, 64,
                                     12, 20,    0, 0, 0, 0,  0,  0};

static const uint16_t i_table[4] = {25, 50, 100, 0};

/************************************************************************************
 * Public variables
 ************************************************************************************/

/************************************************************************************
 * Private functions
 ************************************************************************************/

/************************************************************************************
 * Public functions
 ************************************************************************************/

void atr_init(atr_t *atr) {
  uint8_t x, i;

  atr->TS   = 0;
  atr->T0   = 0;
  atr->nb_T = 0;

  for (x = 0; x < ATR_MAX_INTERFACE; x++) {
    for (i = 0; i < ATR_MAX_PROTOCOL; i++) {
      atr->T[i][x].present = false;
      atr->T[i][x].value   = 0;
    }
  }

  atr->nb_HB = 0;
  for (i = 0; i < ATR_MAX_HISTORICAL; i++) {
    atr->HB[i] = 0;
  }

  atr->TCK.present = false;
  atr->TCK.value   = 0;
}

void iso_params_init(iso_params_t *params) {
  params->state = sc_state_power_off;

  params->frequency        = 4000000;
  params->supported_prot   = 0;
  params->default_protocol = 0;

  atr_init(&(params->ATR));

  params->F    = ATR_DEFAULT_F;
  params->D    = ATR_DEFAULT_D;
  params->fmax = ATR_DEFAULT_FMAX;
  params->N    = ATR_DEFAULT_N;
  params->WI   = ATR_DEFAULT_WI;

  params->Nd   = 0;
  params->Nc   = 0;
  params->DAD  = ATR_DEFAULT_DAD;
  params->SAD  = ATR_DEFAULT_SAD;
  params->WTX  = 0;
  params->IFSC = ATR_DEFAULT_IFS;
  params->IFSD = ATR_DEFAULT_IFS;
  params->BWI  = ATR_DEFAULT_BWI;
  params->CWI  = ATR_DEFAULT_CWI;
  params->EDC  = ATR_DEFAULT_EDC;
}

sc_Status get_Fi(uint8_t i, uint32_t *Fi) {
  if (i > sizeof(f_table) / sizeof(*f_table))
    return sc_Status_Invalid_Parameter;

  *Fi = f_table[i];
  if (*Fi == 0)
    return sc_Status_Invalid_Parameter;

  return sc_Status_Success;
}

sc_Status get_Di(uint8_t i, uint32_t *Di) {
  if (i > sizeof(f_table) / sizeof(*f_table))
    return sc_Status_Invalid_Parameter;

  *Di = d_table[i];
  if (*Di == 0)
    return sc_Status_Invalid_Parameter;

  return sc_Status_Success;
}

sc_Status get_fmax(uint8_t i, uint32_t *fmax) {
  if (i > sizeof(f_table) / sizeof(*f_table))
    return sc_Status_Invalid_Parameter;

  *fmax = fmax_table[i];
  if (*fmax == 0)
    return sc_Status_Invalid_Parameter;

  return sc_Status_Success;
}

sc_Status get_I(uint8_t i, uint32_t *I) {
  if (i > sizeof(f_table) / sizeof(*f_table))
    return sc_Status_Invalid_Parameter;

  *I = i_table[i];
  if (*I == 0)
    return sc_Status_Invalid_Parameter;

  return sc_Status_Success;
}

uint32_t get_min_etu_ns(uint8_t iFi, uint8_t iDi) {
  uint32_t Fi, Di, fmax;

  if (get_Fi(iFi, &Fi) != sc_Status_Success)
    return 0;
  if (get_Di(iDi, &Di) != sc_Status_Success)
    return 0;
  if (get_fmax(iFi, &fmax) != sc_Status_Success)
    return 0;

  return (10000 * Fi) / (Di * (fmax / 100000));
}
