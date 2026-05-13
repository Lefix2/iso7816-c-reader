/*
 * protocol_atr.h
 *
 *  Created on: Feb 27, 2018
 *      Author: ftrefou
 */

#ifndef PROTOCOLS_H_
#define PROTOCOLS_H_

#include "protocol_itf.h"

/************************************************************************************
 * Public defines
 ************************************************************************************/

/************************************************************************************
 * Public types
 ************************************************************************************/

/************************************************************************************
 * Public variables
 ************************************************************************************/

extern protocol_itf_t protocol_atr;

extern protocol_itf_t protocol_pps;

extern protocol_itf_t protocol_APDU_T0;
extern protocol_itf_t protocol_TPDU_T0;

extern protocol_itf_t protocol_APDU_T1;
extern protocol_itf_t protocol_TPDU_T1;

/************************************************************************************
 * Public functions
 ************************************************************************************/

sc_Status atr_get_convention(const atr_t *atr, sc_convention_t *convention);

sc_Status atr_get_Fi_fmax(const atr_t *atr, uint32_t *F, uint32_t *fmax);

sc_Status atr_get_Di(const atr_t *atr, uint32_t *D);

sc_Status atr_get_I(const atr_t *atr, uint32_t *I);

sc_Status atr_get_P(const atr_t *atr, uint8_t *P);

void atr_get_N(const atr_t *atr, uint8_t *N);

sc_Status atr_get_WI(const atr_t *atr, uint8_t *WI);

void atr_T1_specific_get_EDC(const atr_t *atr, uint8_t *EDC);

sc_Status atr_T1_specific_get_IFS(const atr_t *atr, uint8_t *IFS);

sc_Status atr_T1_specific_get_CBWI(const atr_t *atr, uint8_t *CWI, uint8_t *BWI);

#endif /* PROTOCOLS_H_ */
