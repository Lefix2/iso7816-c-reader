/*
 * Smartcard Config
 * ISO7816 lib configuration file
 */

#ifndef __SMARTCARD_CONFIG_H__
#define __SMARTCARD_CONFIG_H__

/************************************************************************************
 * Public defines
 ************************************************************************************/
 
/* Debug control defines */
#if !ENABLE_DEBUG_ISO7816
#undef DEBUG_WARNING
#define DEBUG_WARNING 0
#undef DEBUG_INFO
#define DEBUG_INFO 0
#undef DEBUG_COMM
#define DEBUG_COMM 0
#endif

#define common_memset      memset
#define common_memcpy      memcpy
#define common_memmove     memmove
#define common_memcmp      memcmp
#define common_strlen      strlen
#define common_strcpy      strcpy
#define common_strcmp      strcmp
#define common_sprintf     sprintf
#define common_snprintf    snprintf

#include <stdint.h>
#include <stdbool.h>

#endif /* __SMARTCARD_CONFIG_H__ */
