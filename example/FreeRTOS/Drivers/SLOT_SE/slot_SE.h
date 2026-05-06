/*
 * slot_SE.h
 *
 *  Created on: 26 f�vr. 2018
 *      Author: ftref
 */

#ifndef SLOT_SE_H_
#define SLOT_SE_H_

#include <slot_itf.h>

/* Shared interfaces */
extern slot_itf_t hslot_SE;

/* API functions */

/* Platform interrupt functions */
void					SE_USART_IRQHandler			(void);

#endif /* SLOT_SE_H_ */
