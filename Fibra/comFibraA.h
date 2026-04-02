/**
 ******************************************************************************
 * @file    comFibraA.h
 * @author  Jose Vargas Gonzaga
 * @brief   Cabecera del módulo de comunicación por Fibra Óptica para el NODO A.
 * * Este módulo gestiona la comunicación bidireccional asíncrona con el Nodo B.
 * Utiliza CMSIS-Driver para UART y colas de mensajes de CMSIS-RTOS2.
 *
 * PROTOCOLO DE TRAMAS ESPERADAS:
 * Toda trama debe ir encerrada entre '<' y '>'.
 * - TX (A -> B): <A:START:REACCION>, <A:STOP>, <A:PING>, <A:SLEEP>
 * - RX (B -> A): <B:ACK>, <B:HIT:P2:98>, <B:BATT:15.4>, <B:END>
 ******************************************************************************
 */

#ifndef COM_FIBRA_A_H
#define COM_FIBRA_A_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"                          
#include "Driver_USART.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/* --- Definiciones y Constantes --- */
#define TAM_MAX_TRAMA   64      /*!< Tamaño máximo del buffer de texto de cada trama */
#define FIBRA_SPEED     115200  /*!< Velocidad en baudios para la fibra óptica */
#define MAX_MENSAJES_Q  10      /*!< Capacidad máxima de las colas de TX y RX */

/* --- Funciones Públicas --- */

/**
 * @brief  Inicializa los recursos de la fibra óptica en el Nodo A.
 * Configura la UART, crea las colas y arranca el hilo transmisor.
 * @return 0 si éxito, -1 si error.
 */
int Init_ComFibraA(void);

/**
 * @brief  Encola un mensaje de texto para ser enviado al Nodo B.
 * @param  msg: Cadena de texto a enviar (ej. "<A:START:REACCION>").
 * @return 0 si se encoló correctamente, -1 si la cola está llena.
 */
int ComFibraA_EnviarMensaje(const char* msg);

/**
 * @brief  Lee un mensaje recibido desde el Nodo B.
 * @param  out_msg: Buffer donde se copiará el mensaje recibido.
 * @param  timeout_ms: Tiempo de espera (usar osWaitForever para bloquear).
 * @return 0 si se leyó una trama, -1 si hubo timeout.
 */
int ComFibraA_RecibirMensaje(char* out_msg, uint32_t timeout_ms);

#endif /* COM_FIBRA_A_H */
