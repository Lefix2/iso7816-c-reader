/*
 * Debug_config
 * Configure debug
 */

#ifndef __DEBUG_CONFIG_H__
#define __DEBUG_CONFIG_H__

#include <stdint.h>

/************************************************************************************
 * Public defines
 ************************************************************************************/
/*
    1: CRITICAL  important error messages
    2: WARNING   important but not critical messages
    3: INFO      informative messages
    4: COMM      all the bytes exchanged between the host and the reader
*/
#define ENABLE_DEBUG_CRITICAL
#define ENABLE_DEBUG_WARNING
#define ENABLE_DEBUG_INFO
#define ENABLE_DEBUG_COMM

#define ENABLE_DEBUG_ISO7816        1

/************************************************************************************
 * Public types
 ************************************************************************************/


/************************************************************************************
 * Public variables
 ************************************************************************************/


/************************************************************************************
 * Public functions
 ************************************************************************************/

#endif /* __DEBUG_CONFIG_H__ */
