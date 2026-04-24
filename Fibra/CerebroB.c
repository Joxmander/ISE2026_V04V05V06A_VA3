/**
 ******************************************************************************
 * @file    CerebroB.c
 * @author  Jose Vargas Gonzaga
 * @brief   Orquestador del Nodo B con monitorización del sensor ToF.
 ******************************************************************************
 */

#include "CerebroB.h"
#include "comFibraB.h"
#include "sensorProximidad.h" // Incluyo mi nueva librería
#include "cmsis_os2.h"
#include <stdio.h>

void Hilo_Orquestador_CerebroB(void *argument) {
    TramaFibra_t mensajeRX;
    uint8_t modo_juego = 0;
    uint16_t consumo_simulado = 48; 
    bool jugando = false;
    uint32_t tiempo_inicio_juego = 0;
    
    // Variables para el monitor del sensor
    uint32_t tick_anterior_monitor = 0;

    FibraB_Init();
    // Nota: SensorProximidad_Init() ya se llama en app_main antes de lanzar este hilo

    while (1) {
        // 1. Gestión de la Fibra Óptica (Nodo A -> Nodo B)
        if (osMessageQueueGet(colaFibraRX_B, &mensajeRX, NULL, 50U) == osOK) {
            switch (mensajeRX.tipo_comando) {
                case 0x10: // PING
                    FibraB_SendFrame(0x20, 0x00, (consumo_simulado >> 8), (consumo_simulado & 0xFF));
                    break;

                case 0x30: // INICIAR JUEGO 
                    consumo_simulado = 48; 
                    modo_juego = mensajeRX.payload[0];
                    FibraB_SendFrame(0x31, modo_juego, 0x00, 0x00); // ACK
                    jugando = true;
                    tiempo_inicio_juego = osKernelGetTickCount();
                    break;

                case 0x40: // GO TO SLEEP
                    consumo_simulado = 2; 
                    jugando = false;      
                    FibraB_SendFrame(0x41, 0x00, 0x00, 0x00); // ACK
                    break;
            }
        }
        
        // 2. MONITOR DEL SENSOR (Depuración en tiempo real)
        // Imprimo el estado cada 200ms para no saturar el ITM
        if ((osKernelGetTickCount() - tick_anterior_monitor) >= 200U) {
            tick_anterior_monitor = osKernelGetTickCount();
            
            printf("[CEREBRO-B] ToF: %d mm | Estado: %s\r\n", 
                   ToF_GetDistancia(), 
                   ToF_ManoEnPosicionNeutra() ? "MANO DETECTADA" : "POSICION LIBRE");
        }

        // 3. Lógica de juego (Simulada por ahora)
        if (jugando && ((osKernelGetTickCount() - tiempo_inicio_juego) >= 3000U)) {
            jugando = false; 
            FibraB_SendFrame(0x50, modo_juego, 5, 100); 
        }
    }
}
