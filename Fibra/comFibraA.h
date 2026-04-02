/**
 ******************************************************************************
 * @file    comFibraA.h
 * @author  Jose Vargas Gonzaga
 * @brief   Módulo de comunicación por Fibra Óptica (Protocolo Binario) - NODO A
 * * Utiliza tramas binarias estrictas de 6 bytes para máxima eficiencia,
 * menor latencia y seguridad de memoria (sin uso de strings).
 ******************************************************************************
 */

#ifndef COM_FIBRA_A_H
#define COM_FIBRA_A_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"                          
#include "Driver_USART.h"
#include <stdint.h>
#include <stdbool.h>

/* --- Definiciones del Hardware --- */
#define FIBRA_SPEED       115200 
#define MAX_MENSAJES_Q    10      

/* --- Definiciones del Protocolo Binario --- */
#define TRAMA_START_BYTE  0xAA    // Byte mágico de inicio
#define TRAMA_END_BYTE    0x55    // Byte mágico de fin

/* Tipos de Mensaje */
#define TIPO_COMANDO      0x01    // Del Nodo A al Nodo B (Órdenes)
#define TIPO_EVENTO       0x02    // Del Nodo B al Nodo A (Sensores)

/* Catálogo de Órdenes (Nodo A -> Nodo B) */
#define CMD_START_JUEGO   0x10
#define CMD_STOP_JUEGO    0x11
#define CMD_SLEEP_NODO    0x12

/* Catálogo de Eventos (Nodo B -> Nodo A) */
#define EVT_HIT_SENSOR    0x20
#define EVT_MISS_SENSOR   0x21
#define EVT_FIN_TIEMPO    0x22
#define EVT_BATERIA_MAH   0x23

/* --- ESTRUCTURA DE LA TRAMA (Empaquetado estricto de 6 bytes) --- */
#pragma pack(push, 1) // Fuerza al compilador a no dejar huecos de memoria
typedef struct {
    uint8_t start_byte;   // Siempre 0xAA
    uint8_t tipo;         // TIPO_COMANDO o TIPO_EVENTO
    uint8_t id_accion;    // Ej: CMD_START_JUEGO o EVT_HIT_SENSOR
    uint8_t param1;       // Parametro 1 (Ej: ID del Pad, o Modo de Juego)
    uint8_t param2;       // Parametro 2 (Ej: Fuerza del golpe)
    uint8_t end_byte;     // Siempre 0x55
} TramaFibra_t;
#pragma pack(pop)

/* --- Funciones Públicas --- */

/**
 * @brief  Inicializa UART, colas de RTOS e interrupciones.
 */
int Init_ComFibraA(void);

/**
 * @brief  Encola una trama binaria para ser enviada por hardware.
 * @param  trama: Puntero a la estructura con los datos a enviar.
 */
int ComFibraA_EnviarTrama(const TramaFibra_t* trama);

/**
 * @brief  Extrae una trama validada de la cola de recepción.
 * @param  out_trama: Donde se copiarán los datos recibidos.
 * @param  timeout_ms: osWaitForever para esperar indefinidamente.
 */
int ComFibraA_RecibirTrama(TramaFibra_t* out_trama, uint32_t timeout_ms);

/**
 * @brief  (DEBUG) Imprime el contenido de la trama por el puerto serie USB.
 */
void ComFibraA_DebugPrintTrama(const TramaFibra_t* trama);

#endif /* COM_FIBRA_A_H */
