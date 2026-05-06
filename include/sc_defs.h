/*
 * Sc defs
 * Definitions for smartcard module according to ISO7816
 */

#ifndef SC_DEFS_H_
#define SC_DEFS_H_

#include <stdbool.h>
#include <stddef.h>


#include "sc_status.h"

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

/************************************************************************************
 * Public defines
 ************************************************************************************/

#define SC_Fd    372
#define SC_Dd    1
#define SC_fmaxd 4000000

#define SC_PROTOCOL_T0    0
#define SC_PROTOCOL_T1    1
#define SC_PROTOCOL_T15   15
#define SC_PROTOCOL_AUTO  0xFF
#define SC_EDC_LRC        0
#define SC_EDC_CRC        1
#define SC_DEFAULT_WT_ETU 9600
#define SC_DEFAULT_WT_MS  893

#define ATR_INTERFACE_A 0
#define ATR_INTERFACE_B 1
#define ATR_INTERFACE_C 2
#define ATR_INTERFACE_D 3

#define ATR_MAX_LENGTH     46
#define ATR_MAX_INTERFACE  4
#define ATR_MAX_PROTOCOL   7
#define ATR_MAX_HISTORICAL 15
#define ATR_DEFAULT_FMAX   5000000
#define ATR_DEFAULT_F      372
#define ATR_DEFAULT_D      1
#define ATR_DEFAULT_I      50
#define ATR_DEFAULT_N      0
#define ATR_DEFAULT_P      5
#define ATR_DEFAULT_WI     10
#define ATR_DEFAULT_DAD    0
#define ATR_DEFAULT_SAD    0
#define ATR_DEFAULT_EDC    SC_EDC_LRC
#define ATR_DEFAULT_IFS    32
#define ATR_DEFAULT_CWI    13
#define ATR_DEFAULT_BWI    4

#define PPS_MAX_LENGTH 6
#define PPSS_IDX       0
#define PPS0_IDX       1
#define PPS0_PPS1_PRES 0x10
#define PPS0_PPS2_PRES 0x20
#define PPS0_PPS3_PRES 0x40

#define TPDU_HEADER_SIZE 5
#define APDU_S_MAX_SIZE  (TPDU_HEADER_SIZE + 256 + 2)
#define APDU_E_MAX_SIZE  (TPDU_HEADER_SIZE + 65536 + 2)

#define CLA_IDX 0
#define INS_IDX 1
#define P1_IDX  2
#define P2_IDX  3
#define P3_IDX  4

#define C1_IDX 0
#define C2_IDX 1
#define C3_IDX 2
#define C4_IDX 3
#define C5_IDX 4
#define C6_IDX 5
#define C7_IDX 6

#define LRC_SIZE          1
#define CRC_SIZE          2
#define T1_PROLOGUE_SIZE  3
#define T1_MAX_DATA_SIZE  0xFE
#define T1_MAX_BLOCK_SIZE (T1_PROLOGUE_SIZE + T1_MAX_DATA_SIZE + CRC_SIZE)
#define NAD_IDX           0
#define PCB_IDX           1
#define LEN_IDX           2

#define PCB_I_BLOCK 0x00
#define PCB_I_MORE  0x20

#define PCB_R_BLOCK       0x80
#define PCB_R_ACK         0x00
#define PCB_R_EDC_ERROR   0x01
#define PCB_R_OTHER_ERROR 0x02

#define PCB_S_BLOCK    0xC0
#define PCB_S_RESYNC   0x00
#define PCB_S_IFS      0x01
#define PCB_S_ABORT    0x02
#define PCB_S_WTX      0x03
#define PCB_S_RESPONSE 0x20

#define INS_GET_RESPONSE 0xC0
#define INS_ENVELOPPE    0xC2

/************************************************************************************
 * Public types
 ************************************************************************************/

/* Interface Byte struct typedef */
typedef struct {
  bool    present; /* Indicate IB presence */
  uint8_t value;   /* IB value only if present */
} itfB_t;

/* ATR TypeDef */
typedef struct {

  uint8_t TS;
  uint8_t T0;

  uint8_t nb_T;
  itfB_t  T[ATR_MAX_PROTOCOL][ATR_MAX_INTERFACE];

  uint8_t nb_HB;
  uint8_t HB[ATR_MAX_HISTORICAL];

  itfB_t TCK;
} atr_t;

typedef enum {
  sc_state_power_off = 0x00,
  sc_state_power_on,
  sc_state_reset_low,
  sc_state_reset_high,
  sc_state_negociable,
  sc_state_active,
  sc_state_active_on_t0,
  sc_state_active_on_t1,
  sc_state_default
} sc_state_t;

typedef enum {
  class_A,
  class_B,
  class_C
} sc_class_t;

typedef enum {
  convention_direct,
  convention_reverse
} sc_convention_t;

typedef struct {
  sc_state_t state; /* State of the SE */

  sc_convention_t convention;
  uint16_t        supported_prot;

  atr_t ATR;

  uint32_t frequency;
  uint32_t F;
  uint32_t D;
  uint8_t  N;
  uint32_t fmax;
  uint32_t Fi;
  uint32_t Di;

  uint8_t default_protocol;

  /* T0 */
  uint8_t WI;

  /* T1 */
  uint8_t Nd; /* N(S) of the device */
  uint8_t Nc; /* N(S) of the card */
  uint8_t DAD;
  uint8_t SAD;
  uint8_t WTX;
  uint8_t IFSC;
  uint8_t IFSD;
  uint8_t BWI;
  uint8_t CWI;
  uint8_t EDC;
} iso_params_t;

/************************************************************************************
 * Public variables
 ************************************************************************************/

/************************************************************************************
 * Public functions
 ************************************************************************************/

void atr_init(atr_t *atr);

void iso_params_init(iso_params_t *params);

sc_Status get_Fi(uint8_t i, uint32_t *Fi);

sc_Status get_Di(uint8_t i, uint32_t *Di);

sc_Status get_fmax(uint8_t i, uint32_t *fmax);

sc_Status get_I(uint8_t i, uint32_t *I);

uint32_t get_min_etu_ns(uint8_t iFi, uint8_t iDi);

#endif /* SC_DEFS_H_ */
