/****************************************************************************
 *  Copyright (C) 2020 RoboMaster.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 ***************************************************************************/

#ifndef __SYS_INCLUDES_H__
#define __SYS_INCLUDES_H__

#include "gd32f30x.h"

/* boolean type definitions */
#ifndef TRUE
    #define TRUE                         1               /**< boolean true  */
#endif

#ifndef FALSE
    #define FALSE                        0               /**< boolean fails */
#endif

#ifndef ENABLE
    #define ENABLE                       1
#endif

#ifndef DISABLE
    #define DISABLE                      0
#endif

#define MUTEX_DECLARE(mutex) uint32_t mutex
#define MUTEX_INIT(mutex)    do{mutex = 0;}while(0)
#define MUTEX_LOCK(mutex)    do{__disable_irq();}while(0)
#define MUTEX_UNLOCK(mutex)  do{__enable_irq();}while(0)

#endif // __SYS_INCLUDES_H__