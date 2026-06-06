/**
 ******************************************************************************
 * @file    buzzer.h
 * @author  Equipo R.E.A.C.T.
 * @brief   Driver del buzzer pasivo (TIM3 CH1 sobre PA6).
 *
 * @details SPRINT 1: Módulo NUEVO.
 *          Genera tonos de frecuencia y duración programables. Se usa en:
 *            - Modo RITMO: 3 beeps de aviso antes de la ronda
 *            - Modo INHIBICIÓN: estímulo de 100 ms (440 Hz grave / 1760 Hz agudo)
 *          La función Buzzer_Tono() es no bloqueante: arranca el PWM y
 *          programa un timer one-shot que apaga la salida al expirar la
 *          duración.
 *
 *          Requisitos CubeMX:
 *            - TIM3 Channel 1 en modo PWM Generation CH1
 *            - Pin PA6 con AF2 (TIM3_CH1)
 *            - Reloj base del TIM3: 84 MHz
 *            - (Prescaler y Period los configura el driver en runtime)
 ******************************************************************************
 */

#ifndef BUZZER_H_
#define BUZZER_H_

#include <stdint.h>

/**
 * @brief Inicializa TIM3 CH1, pin PA6 y timer RTOS de apagado.
 *        Llamar UNA vez al arrancar el sistema.
 * @retval 0 OK, -1 error.
 */
int Buzzer_Init(void);

/**
 * @brief Emite un tono de la frecuencia y duración indicadas.
 * @param frecuencia_hz : 100..5000 Hz. Fuera de rango: ignorado.
 * @param duracion_ms   : 10..5000 ms. Se apaga solo al expirar.
 * @note  Si se llama otra vez antes de que termine el anterior,
 *        el nuevo tono sustituye al anterior (no se encolan).
 */
void Buzzer_Tono(uint16_t frecuencia_hz, uint16_t duracion_ms);

/**
 * @brief Detiene inmediatamente cualquier tono en curso.
 */
void Buzzer_Stop(void);

/* Handle externo configurado por CubeMX */
#include "stm32f4xx_hal.h"
extern TIM_HandleTypeDef htim3;

#endif /* BUZZER_H_ */
