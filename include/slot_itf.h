/*
 * Slot ITF
 * Specify slot interface type
 */

#ifndef __SLOT_ITF_H_
#define __SLOT_ITF_H_

#include "sc_defs.h"
#include "sc_status.h"

/************************************************************************************
 * Public defines
 ************************************************************************************/

/************************************************************************************
 * Public types
 ************************************************************************************/

typedef struct {

  sc_Status (*init)(void);

  sc_Status (*deinit)(void);

  sc_Status (*get_state)(bool *present, bool *powered);

  sc_Status (*activate)(sc_class_t class);

  sc_Status (*deactivate)(void);

  sc_Status (*send_byte)(uint8_t byte);

  sc_Status (*send_bytes)(const uint8_t *ptr, uint32_t len);

  sc_Status (*receive_byte)(uint8_t *byte);

  sc_Status (*receive_bytes)(uint8_t *ptr, uint32_t len);

  sc_Status (*set_frequency)(uint32_t frequency);

  sc_Status (*get_frequency)(uint32_t *frequency);

  sc_Status (*set_timeout_etu)(uint32_t timeout);

  sc_Status (*get_timeout_etu)(uint32_t *timeout);

  sc_Status (*set_guardtime_etu)(uint8_t guardtime);

  sc_Status (*get_guardtime_etu)(uint8_t *guardtime);

  sc_Status (*set_convention)(sc_convention_t convention);

  sc_Status (*get_convention)(sc_convention_t *convention);

  sc_Status (*set_F_D)(uint32_t F, uint32_t D);

  sc_Status (*get_F_D)(uint32_t *F, uint32_t *D);

  sc_Status (*set_IFSD)(uint8_t IFSD);

  sc_Status (*get_IFSD)(uint8_t *IFSD);

  sc_Status (*get_min_etu_ns)(uint32_t *etu_ns);

} slot_itf_t;

/************************************************************************************
 * Public variables
 ************************************************************************************/

/************************************************************************************
 * Public functions
 ************************************************************************************/

#endif /* __SLOT_ITF_H_ */
