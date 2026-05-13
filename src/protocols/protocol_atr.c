/*
 * Protocol ATR
 * Expose API for Answer To Reset
 */

#include <string.h>

#include "sc_defs.h"
#include "slot_itf.h"

#include "protocols.h"
#include "sc_debug.h"

/************************************************************************************
 * Private defines
 ************************************************************************************/

/************************************************************************************
 * Private variables
 ************************************************************************************/

/* nb_Tx[Yi] table, number of interface bytes for a given Y */
static const uint8_t nb_Tx_table[16] = {0, 1, 1, 2, 1, 2, 2, 3,
                                        1, 2, 2, 3, 2, 3, 3, 4};

/************************************************************************************
 * Private functions
 ************************************************************************************/

static sc_Status protocol_atr_transact(sc_context_t  *context,
                                       const uint8_t *send_buffer,
                                       uint32_t       send_length,
                                       uint8_t       *receive_buffer,
                                       uint32_t      *receive_length) {
  sc_Status  ret;
  slot_itf_t slot;
  atr_t      atr;

  uint8_t  TDi;
  uint8_t  len, nb_T;
  uint32_t buffer_size = *receive_length;

  (void)send_buffer;
  *receive_length = 0;

  atr_init(&atr);

  if ((send_length != 0) || (context == (void *)0) ||
      (context->slot == (void *)0)) {
    return sc_Status_Invalid_Parameter;
  }
  slot = *(context->slot);

  if (context->params.state != sc_state_reset_high) {
    return sc_Status_Bad_State;
  }

  if (buffer_size < 2) {
    return sc_Status_Buffer_To_Small;
  }

  /* iso 78116-3 8.1 */
  if ((ret = slot.set_timeout_etu(9600)) != sc_Status_Success) {
    return ret;
  }
  if ((ret = slot.set_guardtime_etu(12)) != sc_Status_Success) {
    return ret;
  }

  /* Receiving TS */
  ret = slot.receive_byte(&receive_buffer[(*receive_length)++]);
  if (ret != sc_Status_Success) {
    return ret;
  }

  if (receive_buffer[0] != 0x3B) {
    /* 0x03 is 0x3F in reverse */
    if (receive_buffer[0] == 0x03) {
      slot.set_convention(convention_reverse);
      receive_buffer[0] = 0x3F;
    } else {
      return sc_Status_ATR_Bad_TS;
    }
  }
  atr.TS = receive_buffer[0];

  /* Receiving T0 */
  ret = slot.receive_byte(&receive_buffer[(*receive_length)++]);
  if (ret != sc_Status_Success) {
    return ret;
  }
  atr.T0 = receive_buffer[1];

  TDi  = atr.T0;
  len  = nb_Tx_table[(atr.T0 & 0xF0) >> 4];
  nb_T = 0;

  /* Receive Txi bytes while some remaining*/
  while (len) {

    if ((*receive_length) + len > buffer_size) {
      return sc_Status_Buffer_To_Small;
    }

    ret = slot.receive_bytes(&receive_buffer[*receive_length], len);
    if (ret != sc_Status_Success) {
      return ret;
    }
    *receive_length += len;

    /* If TAi is present */
    if ((TDi & 0x10) == 0x10) {
      atr.T[nb_T][ATR_INTERFACE_A].present = true;
      atr.T[nb_T][ATR_INTERFACE_A].value =
          receive_buffer[*receive_length - len];
      len--;
    }

    /* If TBi is present */
    if ((TDi & 0x20) == 0x20) {
      atr.T[nb_T][ATR_INTERFACE_B].present = true;
      atr.T[nb_T][ATR_INTERFACE_B].value =
          receive_buffer[*receive_length - len];
      len--;
    }

    /* If TCi is present */
    if ((TDi & 0x40) == 0x40) {
      atr.T[nb_T][ATR_INTERFACE_C].present = true;
      atr.T[nb_T][ATR_INTERFACE_C].value =
          receive_buffer[*receive_length - len];
      len--;
    }

    /* If TDi is present */
    if ((TDi & 0x80) == 0x80) {
      atr.T[nb_T][ATR_INTERFACE_D].present = true;
      atr.T[nb_T][ATR_INTERFACE_D].value =
          receive_buffer[*receive_length - len];
      len--;

      TDi             = receive_buffer[*receive_length - 1];
      atr.TCK.present = (TDi & 0x0F) != 0x00;
      len             = nb_Tx_table[(TDi & 0xF0) >> 4];

      nb_T++;
      if (nb_T > ATR_MAX_PROTOCOL) {
        return sc_Status_ATR_Malformed;
      }
    }
  }
  atr.nb_T = nb_T + 1;

  /* Receive historical bytes if presents*/
  len = atr.T0 & 0x0F;
  if (len > 0) {

    if ((*receive_length) + len > buffer_size) {
      return sc_Status_Buffer_To_Small;
    }

    ret = slot.receive_bytes(&receive_buffer[*receive_length], len);
    if (ret != sc_Status_Success) {
      return ret;
    }

    memcpy(atr.HB, &receive_buffer[*receive_length], len);
    *receive_length += len;
    atr.nb_HB = len;
  }

  /* Receive TCK if present */
  if (atr.TCK.present == true) {
    if ((*receive_length) + 1 > buffer_size) {
      return sc_Status_Buffer_To_Small;
    }

    ret = slot.receive_byte(&receive_buffer[(*receive_length)++]);
    if (ret != sc_Status_Success) {
      return ret;
    }
    atr.TCK.value = receive_buffer[(*receive_length) - 1];

    /* Validate TCK: XOR of bytes T0..TCK must equal zero (ISO 7816-3 §8.2.5) */
    uint8_t xor = 0;
    for (uint32_t i = 1; i < *receive_length; i++)
      xor ^= receive_buffer[i];
    if (xor != 0)
      return sc_Status_ATR_Malformed;
  }

  context->params.ATR = atr;

  SC_DBG_COMM("ATR << ", (char *)receive_buffer, *receive_length);

  return sc_Status_Success;
}

/************************************************************************************
 * Public variables
 ************************************************************************************/

protocol_itf_t protocol_atr = {protocol_atr_transact};

/************************************************************************************
 * Public functions
 ************************************************************************************/

sc_Status atr_get_convention(const atr_t *atr, sc_convention_t *convention) {
  if (atr->TS == 0x3B)
    (*convention) = convention_direct;
  else if (atr->TS == 0x3F)
    (*convention) = convention_reverse;
  else
    return (sc_Status_ATR_Malformed);

  return (sc_Status_Success);
}

sc_Status atr_get_Fi_fmax(const atr_t *atr, uint32_t *F, uint32_t *fmax) {
  if (atr->T[0][ATR_INTERFACE_A].present) {
    uint8_t i = (atr->T[0][ATR_INTERFACE_A].value) >> 4;
    return get_iParams(i, 0, F, NULL, fmax);
  }
  (*F)    = ATR_DEFAULT_F;
  (*fmax) = ATR_DEFAULT_FMAX;
  return sc_Status_Success;
}

sc_Status atr_get_Di(const atr_t *atr, uint32_t *D) {
  uint8_t i;

  if (atr->T[0][ATR_INTERFACE_A].present) {
    i = atr->T[0][ATR_INTERFACE_A].value & 0x0F;
    return get_iParams(0, i, NULL, D, NULL);
  } else {
    (*D) = ATR_DEFAULT_D;
  }

  return sc_Status_Success;
}

sc_Status atr_get_I(const atr_t *atr, uint32_t *I) {
  uint8_t i;

  if (atr->T[0][ATR_INTERFACE_B].present) {
    i = (atr->T[0][ATR_INTERFACE_B].value & 0x60) >> 5;
    return get_I(i, I);
  } else {
    (*I) = ATR_DEFAULT_I;
  }

  return sc_Status_Success;
}

sc_Status atr_get_P(const atr_t *atr, uint8_t *P) {
  if (atr->T[1][ATR_INTERFACE_B].present) {
    (*P) = atr->T[1][ATR_INTERFACE_B].value;
  } else if (atr->T[0][ATR_INTERFACE_B].present) {
    (*P) = atr->T[0][ATR_INTERFACE_B].value & 0x1F;
  } else {
    (*P) = ATR_DEFAULT_P;
  }

  return sc_Status_Success;
}

void atr_get_N(const atr_t *atr, uint8_t *N) {
  if (atr->T[0][ATR_INTERFACE_C].present) {
    (*N) = atr->T[0][ATR_INTERFACE_C].value;
  } else {
    (*N) = ATR_DEFAULT_N;
  }
}

sc_Status atr_get_WI(const atr_t *atr, uint8_t *WI) {
  itfB_t TC2 = atr->T[1][ATR_INTERFACE_C];

  if (TC2.present) {
    if (TC2.value == 0) {
      return sc_Status_ATR_Malformed;
    }
    (*WI) = TC2.value;
  } else {
    (*WI) = ATR_DEFAULT_WI;
  }

  return sc_Status_Success;
}

void atr_T1_specific_get_EDC(const atr_t *atr, uint8_t *EDC) {
  for (int i = 2; i < ATR_MAX_PROTOCOL; i++) {
    /* For TD defining T1 */
    if ((atr->T[i][ATR_INTERFACE_C].present) &&
        ((atr->T[i - 1][ATR_INTERFACE_D].value & 0x0F) == SC_PROTOCOL_T1)) {
      *EDC = atr->T[i][ATR_INTERFACE_C].value & 0x01;
      return;
    }
  }
  *EDC = ATR_DEFAULT_EDC;
}

sc_Status atr_T1_specific_get_IFS(const atr_t *atr, uint8_t *IFS) {
  for (int i = 2; i < ATR_MAX_PROTOCOL; i++) {
    /* For TD defining T1 */
    if ((atr->T[i][ATR_INTERFACE_A].present) &&
        ((atr->T[i - 1][ATR_INTERFACE_D].value & 0x0F) == SC_PROTOCOL_T1)) {
      *IFS = atr->T[i][ATR_INTERFACE_A].value;
      if (*IFS == 0x00 || *IFS == 0xFF) {
        return sc_Status_ATR_Malformed;
      }
      return sc_Status_Success;
    }
  }

  *IFS = ATR_DEFAULT_IFS;

  return sc_Status_Success;
}

sc_Status
atr_T1_specific_get_CBWI(const atr_t *atr, uint8_t *CWI, uint8_t *BWI) {
  for (int i = 2; i < ATR_MAX_PROTOCOL; i++) {
    /* For TD defining T1 */
    if ((atr->T[i][ATR_INTERFACE_B].present) &&
        ((atr->T[i - 1][ATR_INTERFACE_D].value & 0x0F) == SC_PROTOCOL_T1)) {
      *CWI = atr->T[i][ATR_INTERFACE_B].value & 0x0F;
      *BWI = (atr->T[i][ATR_INTERFACE_B].value >> 4) & 0x0F;
      if (*BWI > 0x09) {
        return sc_Status_ATR_Malformed;
      }
      return sc_Status_Success;
    }
  }

  *CWI = ATR_DEFAULT_CWI;
  *BWI = ATR_DEFAULT_BWI;

  return sc_Status_Success;
}
