/**
 ******************************************************************************
 * @file    CerebroB.c
 * @author  Jose Vargas Gonzaga
 * @brief   Orquestador del Nodo B. Integración con Power y Monitor.
 ******************************************************************************
 */

#include "CerebroB.h"
#include "comFibraB.h"
#include "power.h"             // Tu modulo de bajo consumo
#include "monitorConsumo.h"    // El nuevo simulador de corriente
#include "cmsis_os2.h"
#include <stdio.h>
#include <stdbool.h>

void Hilo_Orquestador_CerebroB(void *argument) {
    TramaFibra_t mensajeRX;
    
    uint8_t  modo_juego = 0;
    bool     jugando = false;
    
    uint32_t tiempo_inicio_fase = 0;
    uint8_t  fase_actual = 0;
    uint8_t  nivel_alcanzado = 1;
    uint8_t  puntos_actuales = 0;
    uint32_t tick_actual = 0;
    
    FibraB_Init();
    Monitor_Init(); 

    while (1) {
        tick_actual = osKernelGetTickCount();

        // 1. ESCUCHAR ÓRDENES DEL NODO A
        if (osMessageQueueGet(colaFibraRX_B, &mensajeRX, NULL, 50U) == osOK) {
            switch (mensajeRX.tipo_comando) {
                
                case 0x10: // PING
                    // Leemos el valor del módulo de energía y se lo mandamos al Nodo A
                    {
                        uint16_t mA = Monitor_GetConsumo_mA();
                        FibraB_SendFrame(0x20, 0x00, (mA >> 8), (mA & 0xFF));
                    }
                    break;

                case 0x30: // INICIAR JUEGO 
                    Monitor_SetEstado(MODO_PWR_JUEGO); 
                    modo_juego = mensajeRX.payload[0];
                    FibraB_SendFrame(0x31, modo_juego, 0x00, 0x00); 
                    
                    jugando = true;
                    fase_actual = 0;
                    nivel_alcanzado = 1;
                    puntos_actuales = 0;
                    tiempo_inicio_fase = tick_actual;
                    break;

                case 0x40: // GO TO SLEEP 
                    jugando = false;
                    Monitor_SetEstado(MODO_PWR_SLEEP);
                    
                    FibraB_SendFrame(0x41, 0x00, 0x00, 0x00); 
                    
                    // Pequeño delay de RTOS para asegurar que el buffer de UART se vacía 
                    // hacia el Nodo A antes de apagar el cerebro
                    osDelay(10); 
                    
                    // --- DORMIMOS EL MICRO ---
                    Sistema_EntrarEnSleep();
                    
                    // --- AL DESPERTAR (Por interrupcion UART) ---
                    Monitor_SetEstado(MODO_PWR_IDLE);
                    break;
            }
        }
        
        // 2. MÁQUINA DE ESTADOS DE JUEGOS (Cero bloqueos)
        if (jugando && !is_sleeping) {
            switch (modo_juego) {
                case 1: /* MODO 1: Memoria */
                    if (tick_actual - tiempo_inicio_fase >= 3000U) {
                        tiempo_inicio_fase = tick_actual;
                        fase_actual++;
                        nivel_alcanzado = fase_actual;
                        puntos_actuales += 15; 

                        if (fase_actual >= 3) {
                            jugando = false; 
                            Monitor_SetEstado(MODO_PWR_IDLE); 
                            FibraB_SendFrame(0x50, nivel_alcanzado, puntos_actuales, 0x00); 
                        }
                    }
                    break;

                case 2: /* MODO 2: Fuerza */
                    if (tick_actual - tiempo_inicio_fase >= 2000U) {
                        tiempo_inicio_fase = tick_actual;
                        fase_actual++;
                        puntos_actuales += 33; 

                        if (fase_actual >= 3) {
                            jugando = false; 
                            Monitor_SetEstado(MODO_PWR_IDLE);
                            FibraB_SendFrame(0x50, 3, puntos_actuales, 0x00); 
                        }
                    }
                    break;

                case 3: /* MODO 3: Inhibición */
                    if (tick_actual - tiempo_inicio_fase >= 4000U) {
                        tiempo_inicio_fase = tick_actual;
                        fase_actual++;
                        nivel_alcanzado = fase_actual;

                        if (fase_actual >= 3) {
                            jugando = false; 
                            Monitor_SetEstado(MODO_PWR_IDLE);
                            FibraB_SendFrame(0x50, nivel_alcanzado, 0, 0x00); 
                        }
                    }
                    break;

                case 4: /* MODO 4: Ritmo */
                    if (tick_actual - tiempo_inicio_fase >= 1000U) {
                        tiempo_inicio_fase = tick_actual;
                        fase_actual++;
                        puntos_actuales += 10;

                        if (fase_actual >= 6) {
                            jugando = false; 
                            Monitor_SetEstado(MODO_PWR_IDLE);
                            FibraB_SendFrame(0x50, 1, puntos_actuales, 0x00); 
                        }
                    }
                    break;

                default:
                    jugando = false;
                    Monitor_SetEstado(MODO_PWR_IDLE);
                    break;
            }
        }
    }
}
