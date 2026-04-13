/**
 ******************************************************************************
 * @file    CerebroB.c
 * @author  Jose Vargas Gonzaga
 * @brief   Lógica central del Nodo B. 
 * En esta fase, he diseñado el Nodo B para que actúe como Esclavo (Terminal).
 * Su principal característica es la eficiencia energética: el hilo permanece
 * totalmente suspendido hasta que recibe una petición del Nodo A.
 ******************************************************************************
 */

#include "CerebroB.h"
#include "comFibraB.h"
#include "cmsis_os2.h"

void Hilo_Orquestador_CerebroB(void *argument) {
    TramaFibra_t mensajeRX;
    osStatus_t estado;

    // Inicializo mi hardware de comunicación óptica para este nodo
    FibraB_Init();

    while (1) {
        // 1. Ahorro energético radical: Me bloqueo INDEFINIDAMENTE.
        // Uso osWaitForever para ceder el 100% de la CPU al RTOS. 
        // El hilo solo despertará por interrupción cuando llegue una trama válida.
        estado = osMessageQueueGet(colaFibraRX_B, &mensajeRX, NULL, osWaitForever);

        if (estado == osOK) {
            // He despertado porque hay un mensaje. Compruebo si viene del Maestro (0x10).
            if (mensajeRX.tipo_comando == 0x10) {
                
                // TODO: En fases futuras, aquí lanzaré las lecturas físicas de mis 
                // sensores (ADC para piezoeléctricos, I2C para proximidad, etc.).
                // Por ahora, inyecto un dato simulado para validar el enlace bidireccional.
                // Simulo: Pad 1 (0x01) golpeado con una fuerza del 85% (0x55).
                
                // 2. Respuesta inmediata: Devuelvo los datos al Nodo A.
                // Uso el tipo 0x20 para indicarle al Maestro que es mi paquete de respuesta.
                FibraB_SendFrame(0x20, 0x01, 0x55, 0x00);
            }
        }
    }
}
