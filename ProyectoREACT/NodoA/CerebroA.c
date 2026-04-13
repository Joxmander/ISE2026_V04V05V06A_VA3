/**
 ******************************************************************************
 * @file    CerebroA.c
 * @author  Jose Vargas Gonzaga
 * @brief   Lógica central de la Estación Base (Nodo A).
 * En esta iteración de prueba (Fase 3), he configurado este nodo para actuar
 * como Maestro. Mi objetivo es tomar la iniciativa, interrogar al Nodo B
 * y procesar su respuesta basándome en tiempos de guarda (Timeouts).
 ******************************************************************************
 */

#include "CerebroA.h"
#include "comFibraA.h"
#include "cmsis_os2.h"

void Hilo_Orquestador_Cerebro(void *argument) {
    TramaFibra_t mensajeRX;
    osStatus_t estado;

    // Inicializo mi hardware de Fibra Óptica usando la librería que he creado
    FibraA_Init();

    while (1) {
        // 1. Actitud de Maestro: Envío una solicitud de estado/comando al Nodo B.
        // Aquí mando el tipo 0x10 (Petición) pidiendo iniciar el modo de juego 1.
        FibraA_SendFrame(0x10, 0x01, 0x00, 0x00);

        // 2. Desacoplamiento temporal: Me pongo a dormir el hilo esperando la cola.
        // Asigno un timeout estricto de 1 segundo (1000U). Si el Nodo B está roto,
        // desenchufado, o el cable cortado, mi hilo no se quedará colgado para siempre.
        estado = osMessageQueueGet(colaFibraRX, &mensajeRX, NULL, 1000U);

        if (estado == osOK) {
            // El Nodo B ha respondido a tiempo. Valido que sea la respuesta esperada.
            if (mensajeRX.tipo_comando == 0x20) { 
                
                // TODO: En fases posteriores, aquí conectaré estos datos recibidos
                // con las variables react_rx_trama de la web CGI y mi pantalla LCD.
                
                // Genero una pausa deliberada de 1 segundo antes de mi siguiente petición
                // para mantener el "ping-pong" estable y no saturar el canal óptico.
                osDelay(1000U); 
            }
        } else if (estado == osErrorTimeout) {
            // Estrategia de recuperación: Si ha saltado el timeout, asumo pérdida
            // de conexión. Duermo el hilo un instante y en la siguiente iteración
            // del while(1) volveré a intentar enviar el comando.
            osDelay(500U); 
        }
    }
}
