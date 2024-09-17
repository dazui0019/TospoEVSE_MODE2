#ifndef __GD32_HAL_H
#define __GD32_HAL_H

#include "stdlib.h"

#define GD_MAX_DELAY  0xFFFFFFFFU

/** 
  * @brief  GD Status structures definition  
  */  
typedef enum 
{
  GD_OK       = 0x00U,
  GD_ERROR    = 0x01U,
  GD_BUSY     = 0x02U,
  GD_TIMEOUT  = 0x03U
} GD_StatusTypeDef;

#endif
