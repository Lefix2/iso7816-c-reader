/*
 * smartcard
 * API to manage an ISO7816 communication
 */

#include <stdio.h>
#include <string.h>

#include "sc_defs.h"
#include "slot_itf.h"
#include "smartcard.h"

#include "EDC.h"
#include "protocols.h"
#include "sc_debug.h"

/************************************************************************************
 * Private defines
 ************************************************************************************/

/************************************************************************************
 * Private variables
 ************************************************************************************/

static struct reg_t {
  bool         registered;
  sc_context_t context;
} reg_p[SC_MAX_SLOTS];

/************************************************************************************
 * Private functions
 ************************************************************************************/

static sc_Status slot_init(uint32_t slot) {
  /* Init context */
  iso_params_init(&reg_p[slot].context.params);

  /* Init HardWare */
  if (reg_p[slot].context.slot) {
    return reg_p[slot].context.slot->init();
  }

  return sc_Status_Success;
}

static sc_Status slot_deInit(uint32_t slot) {
  /* DeInit HardWare */
  return reg_p[slot].context.slot->deinit();
}

static void initfromatr_default_protocol(iso_params_t *params) {
  uint16_t supported_prot = 0;
  uint8_t  defaultProt    = SC_PROTOCOL_AUTO;
  uint8_t  tmp_prot       = 0;
  uint32_t i;
  atr_t    atr = params->ATR;

  for (i = 0; i < ATR_MAX_PROTOCOL; i++) {
    if (atr.T[i][ATR_INTERFACE_D].present) {
      tmp_prot = atr.T[i][ATR_INTERFACE_D].value & 0x0F;
      supported_prot |= 1 << tmp_prot;
      if (defaultProt == SC_PROTOCOL_AUTO) {
        defaultProt = tmp_prot;
      }
    }
  }

  // Specific mode
  if (atr.T[1][ATR_INTERFACE_A].present) {
    defaultProt = atr.T[1][ATR_INTERFACE_A].value & 0x0F;
    supported_prot |= 1 << defaultProt;
  } else if (defaultProt == SC_PROTOCOL_AUTO) {
    defaultProt = SC_PROTOCOL_T0;
    supported_prot |= 1 << defaultProt;
  }

  params->supported_prot   = supported_prot;
  params->default_protocol = defaultProt;
}

static sc_Status initfromatr_T1_specific(iso_params_t *params) {
  sc_Status ret;
  atr_t     atr = params->ATR;

  /* Init EDC type */
  atr_T1_specific_get_EDC(&atr, &(params->EDC));

  /* Init IFSD/IFSC */
  if ((ret = atr_T1_specific_get_IFS(&atr, &(params->IFSC))) !=
      sc_Status_Success) {
    return ret;
  }
  params->IFSD = params->IFSC;

  /* Init CWI, BWI */
  if ((ret = atr_T1_specific_get_CBWI(&atr, &(params->CWI), &(params->BWI))) !=
      sc_Status_Success) {
    return ret;
  }

  return ret;
}

static sc_Status initfromatr_global(iso_params_t *params) {
  sc_Status ret;

  /* Init WI */
  if ((ret = atr_get_WI(&(params->ATR), &(params->WI))) != sc_Status_Success) {
    return ret;
  }

  /* Init Fi and fmax */
  if ((ret = atr_get_Fi_fmax(&(params->ATR), &(params->Fi), &(params->fmax))) !=
      sc_Status_Success) {
    return ret;
  }

  /* Init Di */
  if ((ret = atr_get_Di(&(params->ATR), &(params->Di))) != sc_Status_Success) {
    return ret;
  }

  /* Init N */
  atr_get_N(&(params->ATR), &(params->N));

  return ret;
}

static sc_Status initfromatr(iso_params_t *params) {
  sc_Status ret;

  /* init default protocol */
  initfromatr_default_protocol(params);

  /* init global parameters*/
  if ((ret = initfromatr_global(params)) != sc_Status_Success) {
    return ret;
  }

  /* init T1 specific parameters */
  if ((ret = initfromatr_T1_specific(params)) != sc_Status_Success) {
    return ret;
  }

  return ret;
}

static sc_Status
prepare_pps(sc_context_t *context, uint8_t *pps, uint32_t *pps_len) {
  iso_params_t *params = &context->params;
  slot_itf_t   *slot   = context->slot;
  atr_t        *atr    = &params->ATR;
  sc_Status     st     = sc_Status_Success;

  pps[PPSS_IDX] = 0xFF;
  pps[PPS0_IDX] = params->default_protocol;
  *pps_len      = 2;

  if (atr->T[0][ATR_INTERFACE_A].present) {

    /* If the card does not try to lower the default speed */
    if (params->Fi / params->Di < ATR_DEFAULT_F / ATR_DEFAULT_D) {

      // negociated F and D indices
      uint8_t iDn, iFn;
      iDn = atr->T[0][ATR_INTERFACE_A].value & 0x0F;
      iFn = (atr->T[0][ATR_INTERFACE_A].value) >> 4;

      // Get Slot min etu
      uint32_t min_etu_ns;

      st = slot->get_min_etu_ns(&min_etu_ns);
      if (st == sc_Status_Success) {
        // Slot has specified a min etu
        uint32_t etu_ns = get_min_etu_ns(iFn, iDn);

        // if min etu is to short
        if (etu_ns < min_etu_ns) {
          // find a compromise, F must be betwee Fd and Fi, D between Dd and Di
          while (etu_ns < min_etu_ns) {
            if (iDn <= 1)
              return sc_Status_PPS_Unsuccessfull;
            iDn--;
            etu_ns = get_min_etu_ns(iFn, iDn);
          }
        }
      } else if (st != sc_Status_Unsuported_feature)
        return st;
      else
        st = sc_Status_Success;

      // pps1
      pps[PPS0_IDX] |= PPS0_PPS1_PRES;
      pps[(*pps_len)++] = (uint8_t)(iFn << 4 | iDn);
    }
  }

  /* pps2 */
  pps[PPS0_IDX] |= PPS0_PPS2_PRES;
  pps[(*pps_len)++] = 0x00; // No support of SPU

  /* pps3 */
  pps[PPS0_IDX] |= PPS0_PPS3_PRES;
  pps[(*pps_len)++] = 0x00;

  /* pck */
  pps[*pps_len] = EDC_LRC(pps, *pps_len);
  (*pps_len)++;

  return st;
}

static sc_Status finalize_pps(sc_context_t *context,
                              uint8_t      *pps,
                              uint32_t      pps_len,
                              uint8_t      *pps_resp,
                              uint32_t      pps_resp_len) {
  UNUSED(pps_len);
  UNUSED(pps_resp_len);

  iso_params_t *params = &context->params;

  uint8_t pps1_pres = (pps[PPS0_IDX] & PPS0_PPS1_PRES) ? 1 : 0;
  uint8_t pps2_pres = (pps[PPS0_IDX] & PPS0_PPS2_PRES) ? 1 : 0;
  uint8_t pps3_pres = (pps[PPS0_IDX] & PPS0_PPS3_PRES) ? 1 : 0;

  uint8_t pps1_resp_pres = (pps_resp[PPS0_IDX] & PPS0_PPS1_PRES) ? 1 : 0;
  uint8_t pps2_resp_pres = (pps_resp[PPS0_IDX] & PPS0_PPS2_PRES) ? 1 : 0;
  uint8_t pps3_resp_pres = (pps_resp[PPS0_IDX] & PPS0_PPS3_PRES) ? 1 : 0;

  // ISO-7816 9.3 1st rule
  if ((pps[PPS0_IDX] & 0x0F) != (pps_resp[PPS0_IDX] & 0x0F))
    return sc_Status_PPS_Unsuccessfull;

  // ISO-7816 9.3 2nd rule
  if (pps1_resp_pres) {
    if (pps1_pres == 0)
      return sc_Status_PPS_Unsuccessfull;

    if (pps_resp[PPS0_IDX + pps1_resp_pres] != pps[PPS0_IDX + pps1_pres])
      return sc_Status_PPS_Unsuccessfull;

    uint8_t iFn = pps_resp[PPS0_IDX + pps1_resp_pres] >> 4;
    uint8_t iDn = pps_resp[PPS0_IDX + pps1_resp_pres] & 0x0F;

    get_iParams(iFn, iDn, &params->F, &params->D, &params->fmax);
  } else {
    params->F    = SC_Fd;
    params->D    = SC_Dd;
    params->fmax = SC_fmaxd;
  }

  // ISO-7816 9.3 3rd rule
  if (pps2_resp_pres) {
    if (pps_resp[PPS0_IDX + pps1_resp_pres + pps2_resp_pres] !=
        pps[PPS0_IDX + pps1_pres + pps2_pres])
      return sc_Status_PPS_Unsuccessfull;
  }

  // ISO-7816 9.3 4th rule
  if (pps3_resp_pres) {
    if (pps_resp[PPS0_IDX + pps1_resp_pres + pps2_resp_pres + pps3_resp_pres] !=
        pps[PPS0_IDX + pps1_pres + pps2_pres + pps3_pres])
      return sc_Status_PPS_Unsuccessfull;
  }

  return sc_Status_Success;
}

/************************************************************************************
 * Public functions
 ************************************************************************************/


sc_Status smartcard_Init(void) {
  uint8_t slotIdx;

  for (slotIdx = 0; slotIdx < SC_MAX_SLOTS; slotIdx++) {
    reg_p[slotIdx].registered   = false;
    reg_p[slotIdx].context.slot = NULL;
    slot_init(slotIdx);
  }

  return sc_Status_Success;
}

sc_Status smartcard_Register_slot(slot_itf_t *slot_interface,
                                  uint32_t   *slot_number) {
  sc_Status ret = sc_Status_Success;
  uint8_t   slotIdx;

  if (!slot_number) {
    return sc_Status_Invalid_Parameter;
  }

  for (slotIdx = 0; slotIdx < SC_MAX_SLOTS; slotIdx++) {
    if (!reg_p[slotIdx].registered) {
      reg_p[slotIdx].registered = true;
      *slot_number              = slotIdx;
      break;
    }
  }

  if (slotIdx >= SC_MAX_SLOTS)
    return sc_Status_Buffer_To_Small;

  reg_p[slotIdx].context.slot = slot_interface;

  if ((ret = slot_init(slotIdx)) != sc_Status_Success) {
    return ret;
  }

  return ret;
}

sc_Status smartcard_UnRegister_slot(uint32_t slot) {
  sc_Status ret;

  if ((slot >= SC_MAX_SLOTS) || (!reg_p[slot].registered)) {
    return sc_Status_Bad_Slot;
  }

  if ((ret = smartcard_Power_Off(slot)) != sc_Status_Success) {
    return ret;
  }

  reg_p[slot].registered = false;

  return slot_deInit(slot);
}

sc_Status smartcard_Power_On(uint32_t  slot,
                             uint8_t   preferred_protocol,
                             uint8_t  *atr,
                             uint32_t *atrlen,
                             uint8_t  *protocol) {
  sc_Status ret;

  sc_class_t    current_class;
  uint8_t       pps[PPS_MAX_LENGTH], pps_resp[PPS_MAX_LENGTH];
  uint32_t      ppslen, ppsresplen;
  sc_context_t *slotContext;

  if ((slot >= SC_MAX_SLOTS) || (!reg_p[slot].registered)) {
    return sc_Status_Bad_Slot;
  }

  slotContext = &(reg_p[slot].context);

  ret = slotContext->slot->init();
  if (ret != sc_Status_Success) {
    return ret;
  }

  current_class = class_A;

  do {
    if ((ret = slotContext->slot->activate(current_class)) !=
        sc_Status_Success) {
      slotContext->slot->deactivate();
      continue;
    }

    slotContext->params.state = sc_state_reset_high;

    ret = protocol_atr.Transact(slotContext, (void *)0, 0, atr, atrlen);
    if (ret != sc_Status_Success) {
      slotContext->slot->deactivate();
    }

  } while ((ret != sc_Status_Success) && (current_class++ != class_C));

  if (ret != sc_Status_Success) {
    return ret;
  }

  /* Update parameters */
  if ((ret = initfromatr(&(slotContext->params))) != sc_Status_Success) {
    return ret;
  }

  /* F and D can be negotiated */
  /* Card in specific mode */
  itfB_t TA2 = slotContext->params.ATR.T[1][ATR_INTERFACE_A];
  if (TA2.present) {

    slotContext->params.state = sc_state_active;

    /* if b5 is present F and D are implicit, this code don't support this
     * feature */
    if (TA2.value & 0x10) {
      return sc_Status_Unsuported_feature;
    }
    slotContext->params.F = slotContext->params.Fi;
    slotContext->params.D = slotContext->params.Di;
  } else {
    /* Card in negotiable mode */
    slotContext->params.state = sc_state_negociable;

    /* Select protocol*/
    if (slotContext->params.supported_prot & (0x01 << preferred_protocol)) {
      slotContext->params.default_protocol = preferred_protocol;
    }

    /* Generate pps*/
    ret = prepare_pps(slotContext, pps, &ppslen);
    if (ret != sc_Status_Success) {
      return ret;
    }

    ppsresplen = sizeof(pps_resp);
    ret =
        protocol_pps.Transact(slotContext, pps, ppslen, pps_resp, &ppsresplen);
    if (ret != sc_Status_Success) {
      return ret;
    }

    ret = finalize_pps(slotContext, pps, ppslen, pps_resp, ppsresplen);
    if (ret != sc_Status_Success) {
      return ret;
    }
  }

  /* Apply parameters to slot */
  ret =
      slotContext->slot->set_F_D(slotContext->params.F, slotContext->params.D);
  if (ret != sc_Status_Success) {
    return ret;
  }

  ret = slotContext->slot->set_frequency(slotContext->params.fmax);
  if (ret != sc_Status_Success) {
    return ret;
  }
  /* Update context to get real frequency */
  slotContext->slot->get_frequency(&(slotContext->params.frequency));

  if (sc_dbg_enabled(SC_DBG_CAT_GENERAL)) {
    char buf[80];
    snprintf(buf, sizeof(buf), "T%d F=%lu D=%lu freq=%lu/%lu Hz",
             (int)slotContext->params.default_protocol,
             (unsigned long)slotContext->params.F,
             (unsigned long)slotContext->params.D,
             (unsigned long)slotContext->params.frequency,
             (unsigned long)slotContext->params.fmax);
    sc_dbg("power_on", NULL, 0);
    sc_dbg("power_on_params", (const uint8_t *)buf, (uint32_t)strlen(buf));
  }

  /* Update card state*/
  if (slotContext->params.default_protocol == SC_PROTOCOL_T0) {
    *protocol                 = SC_PROTOCOL_T0;
    slotContext->params.state = sc_state_active_on_t0;
  } else {
    *protocol                 = SC_PROTOCOL_T1;
    slotContext->params.state = sc_state_active_on_t1;
  }

  return sc_Status_Success;
}

sc_Status smartcard_Power_Off(uint32_t slot) {
  if ((slot >= SC_MAX_SLOTS) || (!reg_p[slot].registered)) {
    return sc_Status_Bad_Slot;
  }

  reg_p[slot].context.params.state = sc_state_power_off;

  return reg_p[slot].context.slot->deactivate();
}

sc_Status smartcard_Xfer_Data(uint32_t       slot,
                              const uint8_t *send_buffer,
                              uint32_t       send_length,
                              uint8_t       *receive_buffer,
                              uint32_t      *receive_length) {
  sc_Status     ret;
  sc_context_t *slotContext;

  if ((slot >= SC_MAX_SLOTS) || (!reg_p[slot].registered)) {
    return sc_Status_Bad_Slot;
  }

  slotContext = &(reg_p[slot].context);

  switch (reg_p[slot].context.params.state) {
  case sc_state_active_on_t0:
    ret = protocol_APDU_T0.Transact(slotContext, send_buffer, send_length,
                                    receive_buffer, receive_length);
    break;

  case sc_state_active_on_t1:
    ret = protocol_APDU_T1.Transact(slotContext, send_buffer, send_length,
                                    receive_buffer, receive_length);
    break;

  default:
    *receive_length = 0;
    ret             = sc_Status_Bad_State;
    break;
  }

  return ret;
}

bool smartcard_Is_Present(uint32_t slot) {
  bool present, powered;

  if ((slot >= SC_MAX_SLOTS) || (!reg_p[slot].registered)) {
    return false;
  }

  reg_p[slot].context.slot->get_state(&present, &powered);

  return present;
}

bool smartcard_Is_Powered(uint32_t slot) {
  bool present, powered;

  if ((slot >= SC_MAX_SLOTS) || (!reg_p[slot].registered)) {
    return false;
  }

  reg_p[slot].context.slot->get_state(&present, &powered);

  return powered;
}
