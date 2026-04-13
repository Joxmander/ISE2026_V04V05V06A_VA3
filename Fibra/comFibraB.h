/**
 ******************************************************************************
 * @file    comFibraB.h
 * @author  Jose Vargas Gonzaga
 * @brief   Módulo de comunicación por Fibra Óptica - NODO B (R.E.A.C.T.)
 * Define las constantes y la estructura de mi trama de datos personalizada,
 * garantizando la compatibilidad exacta con el decodificador del Nodo A.
 ******************************************************************************
 */

#ifndef COMFIBRA_B_H
#define COMFIBRA_B_H

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os2.h"

// Mantengo mi caracter de sincronización y el tamaño fijo de la trama
#define SOF_BYTE 0xAA
#define FRAME_SIZE 6

// Estructura de la trama parseada y validada
typedef struct {
    uint8_t tipo_comando;
    uint8_t payload[3];
} TramaFibra_t;

// Identificador de la cola de recepción de este nodo
extern osMessageQueueId_t colaFibraRX_B;

// Mis prototipos públicos
void FibraB_Init(void);
void FibraB_SendFrame(uint8_t tipo, uint8_t p1, uint8_t p2, uint8_t p3);

#endif /* COMFIBRA_B_H */
