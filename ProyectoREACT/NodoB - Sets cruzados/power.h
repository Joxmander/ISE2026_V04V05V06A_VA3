#ifndef __POWER_H
#define __POWER_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

extern volatile uint8_t is_sleeping;

void Sistema_EntrarEnSleep(void);
void Apagar_Perifericos_Temporalmente(void);
void Restaurar_Perifericos_Temporalmente(void);
    
#endif
