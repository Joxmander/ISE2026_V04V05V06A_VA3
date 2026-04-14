/**
 ******************************************************************************
 * @file    CerebroB.c
 * @author  Jose Vargas Gonzaga
 * @brief   FASE 5.1: Máquina de estados no bloqueante (Simulador Hardware)
 ******************************************************************************
 */

#include "CerebroB.h"
#include "comFibraB.h"
#include "cmsis_os2.h"

void Hilo_Orquestador_CerebroB(void *argument) {
    TramaFibra_t mensajeRX;
    uint8_t modo_juego = 0;
    uint16_t consumo_simulado = 48; 
    
    // Variables para la máquina de estados de simulación
    bool jugando = false;
    uint32_t tiempo_inicio_juego = 0;

    FibraB_Init();

    while (1) {
        // En lugar de osWaitForever, esperamos 100ms. Si no llega nada,
        // salimos del if y el microprocesador puede procesar otras cosas.
        if (osMessageQueueGet(colaFibraRX_B, &mensajeRX, NULL, 100U) == osOK) {
            
            switch (mensajeRX.tipo_comando) {
                case 0x10: // PING
                    FibraB_SendFrame(0x20, 0x00, (consumo_simulado >> 8), (consumo_simulado & 0xFF));
                    break;

                case 0x30: // INICIAR JUEGO (¡DESPIERTA!)
                    consumo_simulado = 48; // El sistema sale del Sleep automáticamente
                    modo_juego = mensajeRX.payload[0];
                    
                    FibraB_SendFrame(0x31, modo_juego, 0x00, 0x00); // ACK
                    
                    // Iniciamos el cronómetro interno (Sin usar osDelay)
                    jugando = true;
                    tiempo_inicio_juego = osKernelGetTickCount();
                    break;

                case 0x40: // GO TO SLEEP
                    consumo_simulado = 2; // Bajamos consumo
                    jugando = false;      // Si estaba jugando, lo cancelamos
                    FibraB_SendFrame(0x41, 0x00, 0x00, 0x00); // ACK
                    break;
            }
        }
        
        // --- PROCESOS EN SEGUNDO PLANO (Máquina de estados) ---
        // Si estamos en modo "Jugando" y han pasado 2500 milisegundos...
        if (jugando && ((osKernelGetTickCount() - tiempo_inicio_juego) >= 2500U)) {
            jugando = false; // Terminamos la partida
            uint8_t nivel_alcanzado = 5;
            
            // Enviamos la trama final de golpe
            FibraB_SendFrame(0x50, modo_juego, nivel_alcanzado, 100); 
        }
    }
}
