#ifndef MONITOR_CONSUMO_H
#define MONITOR_CONSUMO_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

typedef enum {
    MODO_PWR_IDLE = 0,  
    MODO_PWR_JUEGO,     
    MODO_PWR_SLEEP      
} EstadoEnergia_t;

void Monitor_Init(void);
void Monitor_SetEstado(EstadoEnergia_t estado);
uint16_t Monitor_GetConsumo_mA(void);

#endif /* MONITOR_CONSUMO_H */
