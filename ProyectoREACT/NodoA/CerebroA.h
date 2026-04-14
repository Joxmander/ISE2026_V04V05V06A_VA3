/**
 ******************************************************************************
 * @file    CerebroA.h
 * @author  Jose Vargas Gonzaga
 * @brief   Cabecera del Orquestador de la Estación Base R.E.A.C.T.
 ******************************************************************************
 */

#ifndef CEREBRO_A_H
#define CEREBRO_A_H

#include <stdint.h>
#include "cmsis_os2.h"

// Definimos la estructura del mensaje para conectarnos con la Web (CGI)
typedef struct {
    uint8_t origen;         // 0 = Web, 1 = Joystick, 2 = FibraRX
    uint8_t tipo_comando;   // 1 = Modo Juego, 2 = Trama Debug Manual, 3 = Bajo Consumo
    uint8_t datos[6];       // Datos o Trama cruda
} MensajeCerebro_t;

// Exponemos la cola globalmente
extern osMessageQueueId_t colaEventosCerebro;

void Hilo_Orquestador_Cerebro(void *argument);

#endif /* CEREBRO_A_H */
