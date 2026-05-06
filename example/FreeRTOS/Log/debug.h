/*
 * Debug
 * API for logging events and messages
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include "debug_config.h"

#include <stdio.h>
#include <stdint.h>

/************************************************************************************
 * Public defines
 ************************************************************************************/

#define dbg(fmt, args...)               dbg_printf(fmt, ##args)

/* Critical */
#ifdef ENABLE_DEBUG_CRITICAL
    #define DEBUG_CRITICAL 1
#else
    #define DEBUG_CRITICAL 0
#endif
#define dbg_critical(fmt, args...)              do {if(DEBUG_CRITICAL) dbg("CRITICAL %s:%d: "fmt"\n\r", __FUNCTION__, __LINE__, ##args);} while(0)

/* Warning */
#ifdef ENABLE_DEBUG_WARNING
    #define DEBUG_WARNING 1
#else
    #define DEBUG_WARNING 0
#endif
#define dbg_warning(fmt, args...)               do {if(DEBUG_WARNING) dbg("WARNING %s:%d: "fmt"\n\r", __FUNCTION__, __LINE__, ##args);} while(0)

/* Info */
#ifdef ENABLE_DEBUG_INFO
    #define DEBUG_INFO 1
#else
    #define DEBUG_INFO 0
#endif
#define dbg_info(fmt, args...)                  do {if(DEBUG_INFO) dbg("%s:%d: "fmt"\n\r",__FUNCTION__, __LINE__, ##args);} while(0)
#define dbg_buff_info(info, buffAdd, len)       do {if(DEBUG_COMM) dbg_buff(info, buffAdd, len);} while(0)

/* Comm */
#ifdef ENABLE_DEBUG_COMM
    #define DEBUG_COMM 1
#else
    #define  DEBUG_COMM 0
#endif
#define dbg_comm(fmt, args...)                  do {if(DEBUG_COMM) dbg(fmt, ##args);} while(0)
#define dbg_buff_comm(info, buffAdd, len)       do {if(DEBUG_COMM) dbg_buff(info, buffAdd, len);} while(0)


/************************************************************************************
 * Public types
 ************************************************************************************/


/************************************************************************************
 * Public variables
 ************************************************************************************/


/************************************************************************************
 * Public functions
 ************************************************************************************/

#ifdef DEBUG

    void    dbg_Init                    ( void );
    void    dbg_Deinit                  ( void );
    void    dbg_printf                  ( const char *format, ...);
    void    dbg_buff                    ( const char* info, const char* buffAdd, int len);
    void    dbg_len                     ( const char *s, int len );

#else

    #define dbg_Init()                  do { } while (0)
    #define dbg_Deinit()                do { } while (0)
    #define dbg_printf(...)             do { } while (0)
    #define dbg_buff(...)               do { } while (0)
    #define dbg_len(...)                do { } while (0)

#endif /* DEBUG */


#endif /* __DEBUG_H__ */
