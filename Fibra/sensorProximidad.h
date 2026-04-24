/**
 ******************************************************************************
 * @file    sensorProximidad.h
 * @author  Jose Vargas Gonzaga
 * @brief   Cabecera del módulo del sensor VL53L0X (ToF).
 ******************************************************************************
 */

#ifndef SENSOR_PROXIMIDAD_H
#define SENSOR_PROXIMIDAD_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdint.h>

// Inicializa el hardware y arranca el hilo
void SensorProximidad_Init(void);

// Getter de la distancia bruta (por si hiciera falta en el futuro)
uint16_t ToF_GetDistancia(void);

// Getter procesado para el CerebroB: Devuelve 1 si la mano está en el sensor, 0 si no.
uint8_t ToF_ManoEnPosicionNeutra(void);

#endif /* SENSOR_PROXIMIDAD_H */
