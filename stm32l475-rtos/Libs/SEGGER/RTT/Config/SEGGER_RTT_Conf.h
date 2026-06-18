/*********************************************************************
*                   (c) SEGGER Microcontroller GmbH                  *
*                        The Embedded Experts                        *
*                           www.segger.com                           *
**********************************************************************
*                                                                    *
*        SEGGER RTT * Real Time Transfer for embedded targets        *
*                  https://github.com/SEGGERMicro/RTT                *
*                                                                    *
**********************************************************************

---------------------------END-OF-HEADER------------------------------
Purpose : User configuration file for RTT.
          For available configuration,
          refer to SEGGER_RTT_ConfDefaults.h.
          
          ----------------------------------------------------------------------
          */
          
#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/

#define SEGGER_RTT_SECTION ".segger_rtt"
#define SEGGER_RTT_BUFFER_SECTION ".segger_rtt"


#define BUFFER_SIZE_UP                      (4096)
#define BUFFER_SIZE_DOWN                    (64)

#endif
/*************************** End of file ****************************/