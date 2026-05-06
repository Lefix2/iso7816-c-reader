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

sc_Status           atr_get_convention                  (   atr_t* atr,
                                                            sc_convention_t *convention
                                                        );

sc_Status           atr_get_fmax                        (   atr_t* atr,
                                                            uint32_t* fmax
                                                        );

sc_Status           atr_get_Fi                          (   atr_t* atr,
                                                            uint32_t* F
                                                        );

sc_Status           atr_get_Di                          (   atr_t* atr,
                                                            uint32_t* D
                                                        );

sc_Status           atr_get_I                           (   atr_t* atr,
                                                            uint32_t* I
                                                        );

sc_Status           atr_get_P                           (   atr_t* atr,
                                                            uint8_t* P
                                                        );

sc_Status           atr_get_N                           (   atr_t* atr,
                                                            uint8_t* N
                                                        );

sc_Status           atr_get_WI                          (   atr_t* atr,
                                                            uint8_t* WI
                                                        );

sc_Status           atr_T1_specific_get_EDC             (   atr_t* atr,
                                                            uint8_t* EDC
                                                        );

sc_Status           atr_T1_specific_get_IFS             (   atr_t* atr,
                                                            uint8_t* IFS
                                                        );

sc_Status           atr_T1_specific_get_CBWI            (   atr_t* atr,
                                                            uint8_t* CWI,
                                                            uint8_t* BWI
                                                        );

#endif /* PROTOCOLS_H_ */
